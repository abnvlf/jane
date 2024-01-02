#include "include/codegen.hpp"
#include "../config.h"
#include "include/buffer.hpp"
#include "include/error.hpp"
#include "include/hash_map.hpp"
#include "include/jane_llvm.hpp"
#include "include/list.hpp"
#include "include/os.hpp"
#include "include/parser.hpp"
#include "include/semantic_info.hpp"
#include "include/util.hpp"

#include <llvm-c/Analysis.h>
#include <llvm-c/Core.h>
#include <llvm-c/TargetMachine.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Metadata.h>
#include <llvm/TargetParser/Triple.h>
#include <stdio.h>

#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DiagnosticInfo.h>
#include <llvm/IR/DiagnosticPrinter.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/TargetParser.h>
#include <llvm/Target/TargetMachine.h>

CodeGen *create_codegen(AstNode *root, Buf *in_full_path) {
  CodeGen *g = allocate<CodeGen>(1);
  g->root = root;
  g->fn_table.init(32);
  g->str_table.init(32);
  g->type_table.init(32);
  g->link_table.init(32);
  g->is_static = false;
  g->build_type = CodeGenBuildTypeDebug;
  g->strip_debug_symbols = false;
  g->out_name = nullptr;
  g->out_type = OutTypeUnknown;
  os_path_split(in_full_path, &g->in_dir, &g->in_file);
  return g;
}

void codegen_set_build_type(CodeGen *g, CodeGenBuildType build_type) {
  g->build_type = build_type;
}

void codegen_set_is_static(CodeGen *g, bool is_static) {
  g->is_static = is_static;
}

void codegen_set_strip(CodeGen *g, bool strip) {
  g->strip_debug_symbols = strip;
}

void codegen_set_out_type(CodeGen *g, OutType out_type) {
  g->out_type = out_type;
}

void codegen_set_out_name(CodeGen *g, Buf *out_name) { g->out_name = out_name; }

static LLVMValueRef gen_expr(CodeGen *g, AstNode *expr_node);

static LLVMTypeRef to_llvm_type(AstNode *type_node) {
  assert(type_node->type == NodeTypeType);
  assert(type_node->codegen_node);
  assert(type_node->codegen_node->data.type_node.entry);
  return type_node->codegen_node->data.type_node.entry->type_ref;
}

static LLVMJaneDIType *to_llvm_debug_type(AstNode *type_node) {
  assert(type_node->type == NodeTypeType);
  assert(type_node->codegen_node);
  assert(type_node->codegen_node->data.type_node.entry);
  return type_node->codegen_node->data.type_node.entry->di_type;
}

static bool type_is_unreachable(AstNode *type_node) {
  assert(type_node->type == NodeTypeType);
  assert(type_node->codegen_node);
  assert(type_node->codegen_node->data.type_node.entry);
  return type_node->codegen_node->data.type_node.entry->id == TypeIdUnreachable;
}

static void add_debug_source_node(CodeGen *g, AstNode *node) {
  LLVMJaneSetCurrentDebugLocation(g->builder, node->line + 1, node->column + 1,
                                  g->block_scopes.last());
}

static LLVMValueRef find_or_create_string(CodeGen *g, Buf *str) {
  auto entry = g->str_table.maybe_get(str);
  if (entry) {
    return entry->value;
  }
  LLVMValueRef text = LLVMConstString(buf_ptr(str), buf_len(str), false);
  LLVMValueRef global_value = LLVMAddGlobal(g->module, LLVMTypeOf(text), "");
  LLVMSetLinkage(global_value, LLVMPrivateLinkage);
  LLVMSetInitializer(global_value, text);
  LLVMSetGlobalConstant(global_value, true);
  LLVMSetUnnamedAddr(global_value, true);
  g->str_table.put(str, global_value);
  return global_value;
}

static LLVMValueRef get_variable_value(CodeGen *g, Buf *name) {
  assert(g->cur_fn->proto_node->type == NodeTypeFnProto);
  int param_count = g->cur_fn->proto_node->data.fn_proto.params.length;
  for (int i = 0; i < param_count; i += 1) {
    AstNode *param_decl_node =
        g->cur_fn->proto_node->data.fn_proto.params.at(i);
    assert(param_decl_node->type == NodeTypeParamDecl);
    Buf *param_name = &param_decl_node->data.param_decl.name;
    if (buf_eql_buf(name, param_name)) {
      CodeGenNode *codegen_node = g->cur_fn->fn_def_node->codegen_node;
      assert(codegen_node);
      FnDefNode *codegen_fn_def = &codegen_node->data.fn_def_node;
      return codegen_fn_def->params[i];
    }
  }
  jane_unreachable();
}

static LLVMValueRef gen_fn_call_expr(CodeGen *g, AstNode *node) {
  assert(node->type == NodeTypeFnCallExpr);
  Buf *name = hack_get_fn_call_name(g, node->data.fn_call_expr.fn_ref_expr);
  FnTableEntry *fn_table_entry = g->fn_table.get(name);
  assert(fn_table_entry->proto_node->type == NodeTypeFnProto);
  int expected_param_count =
      fn_table_entry->proto_node->data.fn_proto.params.length;
  int actual_param_count = node->data.fn_call_expr.params.length;
  assert(expected_param_count == actual_param_count);
  LLVMValueRef *param_values = allocate<LLVMValueRef>(actual_param_count);
  for (int i = 0; i < actual_param_count; i += 1) {
    AstNode *expr_node = node->data.fn_call_expr.params.at(i);
    param_values[i] = gen_expr(g, expr_node);
  }
  add_debug_source_node(g, node);
  LLVMValueRef result = LLVMJaneBuildCall(
      g->builder, fn_table_entry->fn_value, param_values, actual_param_count,
      fn_table_entry->calling_convention, "");
  if (type_is_unreachable(
          fn_table_entry->proto_node->data.fn_proto.return_type)) {
    return LLVMBuildUnreachable(g->builder);
  } else {
    return result;
  }
}

static LLVMValueRef gen_prefix_op_expr(CodeGen *g, AstNode *node) {
  assert(node->type == NodeTypePrefixOpExpr);
  assert(node->data.prefix_op_expr.primary_expr);
  LLVMValueRef expr = gen_expr(g, node->data.prefix_op_expr.primary_expr);

  switch (node->data.prefix_op_expr.prefix_op) {
  case PrefixOpNegation:
    add_debug_source_node(g, node);
    return LLVMBuildNeg(g->builder, expr, "");
  case PrefixOpBoolNot: {
    LLVMValueRef zero = LLVMConstNull(LLVMTypeOf(expr));
    add_debug_source_node(g, node);
    return LLVMBuildICmp(g->builder, LLVMIntEQ, expr, zero, "");
  }
  case PrefixOpBinNot:
    add_debug_source_node(g, node);
    return LLVMBuildNot(g->builder, expr, "");
  case PrefixOpInvalid:
    jane_unreachable();
  }
  jane_unreachable();
}

static LLVMValueRef gen_cast_expr(CodeGen *g, AstNode *node) {
  assert(node->type == NodeTypeCastExpr);
  LLVMValueRef expr = gen_expr(g, node->data.cast_expr.prefix_op_expr);
  if (!node->data.cast_expr.type) {
    return expr;
  }
  jane_panic("TODO: casting expression");
}

static LLVMValueRef gen_arithmetic_bin_op_expr(CodeGen *g, AstNode *node) {
  assert(node->type == NodeTypeBinOpExpr);
  LLVMValueRef val1 = gen_expr(g, node->data.bin_op_expr.op1);
  LLVMValueRef val2 = gen_expr(g, node->data.bin_op_expr.op2);

  switch (node->data.bin_op_expr.bin_op) {
  case BinOpTypeBinOr:
    add_debug_source_node(g, node);
    return LLVMBuildOr(g->builder, val1, val2, "");
  case BinOpTypeBinXor:
    add_debug_source_node(g, node);
    return LLVMBuildXor(g->builder, val1, val2, "");
  case BinOpTypeBinAnd:
    add_debug_source_node(g, node);
    return LLVMBuildAnd(g->builder, val1, val2, "");
  case BinOpTypeBitShiftLeft:
    add_debug_source_node(g, node);
    return LLVMBuildShl(g->builder, val1, val2, "");
  case BinOpTypeBitShiftRight:
    add_debug_source_node(g, node);
    return LLVMBuildLShr(g->builder, val1, val2, "");
  case BinOpTypeAdd:
    add_debug_source_node(g, node);
    return LLVMBuildAdd(g->builder, val1, val2, "");
  case BinOpTypeSub:
    add_debug_source_node(g, node);
    return LLVMBuildSub(g->builder, val1, val2, "");
  case BinOpTypeMult:
    add_debug_source_node(g, node);
    return LLVMBuildMul(g->builder, val1, val2, "");
  case BinOpTypeDiv:
    add_debug_source_node(g, node);
    return LLVMBuildSDiv(g->builder, val1, val2, "");
  case BinOpTypeMod:
    add_debug_source_node(g, node);
    return LLVMBuildSRem(g->builder, val1, val2, "");
  case BinOpTypeBoolOr:
  case BinOpTypeBoolAnd:
  case BinOpTypeCmpEq:
  case BinOpTypeCmpNotEq:
  case BinOpTypeCmpLessThan:
  case BinOpTypeCmpGreaterThan:
  case BinOpTypeCmpLessOrEq:
  case BinOpTypeCmpGreaterOrEq:
  case BinOpTypeInvalid:
    jane_unreachable();
  }
  jane_unreachable();
}

static LLVMIntPredicate cmp_op_to_int_predicate(BinOpType cmp_op,
                                                bool is_signed) {
  switch (cmp_op) {
  case BinOpTypeCmpEq:
    return LLVMIntEQ;
  case BinOpTypeCmpNotEq:
    return LLVMIntNE;
  case BinOpTypeCmpLessThan:
    return is_signed ? LLVMIntSLT : LLVMIntULT;
  case BinOpTypeCmpGreaterThan:
    return is_signed ? LLVMIntSGT : LLVMIntUGT;
  case BinOpTypeCmpLessOrEq:
    return is_signed ? LLVMIntSLE : LLVMIntULE;
  case BinOpTypeCmpGreaterOrEq:
    return is_signed ? LLVMIntSGE : LLVMIntUGE;
  default:
    jane_unreachable();
  }
}

static LLVMValueRef gen_cmp_expr(CodeGen *g, AstNode *node) {
  assert(node->type == NodeTypeBinOpExpr);
  LLVMValueRef val1 = gen_expr(g, node->data.bin_op_expr.op1);
  LLVMValueRef val2 = gen_expr(g, node->data.bin_op_expr.op2);
  LLVMIntPredicate pred =
      cmp_op_to_int_predicate(node->data.bin_op_expr.bin_op, true);
  add_debug_source_node(g, node);
  return LLVMBuildICmp(g->builder, pred, val1, val2, "");
}

static LLVMValueRef gen_bool_and_expr(CodeGen *g, AstNode *node) {
  assert(node->type == NodeTypeBinOpExpr);
  LLVMValueRef val1 = gen_expr(g, node->data.bin_op_expr.op1);
  LLVMBasicBlockRef true_block =
      LLVMAppendBasicBlock(g->cur_fn->fn_value, "BoolAndTrue");
  LLVMBasicBlockRef false_block =
      LLVMAppendBasicBlock(g->cur_fn->fn_value, "BoolAndFalse");
  LLVMValueRef zero = LLVMConstNull(LLVMTypeOf(val1));
  add_debug_source_node(g, node);
  LLVMValueRef val1_i1 = LLVMBuildICmp(g->builder, LLVMIntEQ, val1, zero, "");
  LLVMBuildCondBr(g->builder, val1_i1, false_block, true_block);

  LLVMPositionBuilderAtEnd(g->builder, true_block);
  LLVMValueRef val2 = gen_expr(g, node->data.bin_op_expr.op2);
  add_debug_source_node(g, node);
  LLVMValueRef val2_i1 = LLVMBuildICmp(g->builder, LLVMIntEQ, val2, zero, "");

  LLVMPositionBuilderAtEnd(g->builder, false_block);
  add_debug_source_node(g, node);
  LLVMValueRef phi = LLVMBuildPhi(g->builder, LLVMInt1Type(), "");
  LLVMValueRef one_i1 = LLVMConstAllOnes(LLVMInt1Type());
  LLVMValueRef incoming_values[2] = {one_i1, val2_i1};
  LLVMBasicBlockRef incoming_blocks[2] = {LLVMGetInsertBlock(g->builder),
                                          true_block};
  LLVMAddIncoming(phi, incoming_values, incoming_blocks, 2);
  return phi;
}

static LLVMValueRef gen_bool_or_expr(CodeGen *g, AstNode *expr_node) {
  assert(expr_node->type == NodeTypeBinOpExpr);
  LLVMValueRef val1 = gen_expr(g, expr_node->data.bin_op_expr.op1);
  LLVMBasicBlockRef false_block =
      LLVMAppendBasicBlock(g->cur_fn->fn_value, "BoolOrFalse");
  LLVMBasicBlockRef true_block =
      LLVMAppendBasicBlock(g->cur_fn->fn_value, "BoolOrTrue");

  LLVMValueRef zero = LLVMConstNull(LLVMTypeOf(val1));
  add_debug_source_node(g, expr_node);
  LLVMValueRef val1_i1 = LLVMBuildICmp(g->builder, LLVMIntEQ, val1, zero, "");
  LLVMBuildCondBr(g->builder, val1_i1, false_block, true_block);

  LLVMPositionBuilderAtEnd(g->builder, false_block);
  LLVMValueRef val2 = gen_expr(g, expr_node->data.bin_op_expr.op2);
  add_debug_source_node(g, expr_node);
  LLVMValueRef val2_i1 = LLVMBuildICmp(g->builder, LLVMIntEQ, val2, zero, "");

  LLVMPositionBuilderAtEnd(g->builder, true_block);
  add_debug_source_node(g, expr_node);
  LLVMValueRef phi = LLVMBuildPhi(g->builder, LLVMInt1Type(), "");
  LLVMValueRef one_i1 = LLVMConstAllOnes(LLVMInt1Type());
  LLVMValueRef incoming_values[2] = {one_i1, val2_i1};
  LLVMBasicBlockRef incoming_blocks[2] = {LLVMGetInsertBlock(g->builder),
                                          false_block};
  LLVMAddIncoming(phi, incoming_values, incoming_blocks, 2);

  return phi;
}

static LLVMValueRef gen_bin_op_expr(CodeGen *g, AstNode *node) {
  switch (node->data.bin_op_expr.bin_op) {
  case BinOpTypeInvalid:
    jane_unreachable();
  case BinOpTypeBoolOr:
    return gen_bool_or_expr(g, node);
  case BinOpTypeBoolAnd:
    return gen_bool_and_expr(g, node);
  case BinOpTypeCmpEq:
  case BinOpTypeCmpNotEq:
  case BinOpTypeCmpLessThan:
  case BinOpTypeCmpGreaterThan:
  case BinOpTypeCmpLessOrEq:
  case BinOpTypeCmpGreaterOrEq:
    return gen_cmp_expr(g, node);
  case BinOpTypeBinOr:
  case BinOpTypeBinXor:
  case BinOpTypeBinAnd:
  case BinOpTypeBitShiftLeft:
  case BinOpTypeBitShiftRight:
  case BinOpTypeAdd:
  case BinOpTypeSub:
  case BinOpTypeMult:
  case BinOpTypeDiv:
  case BinOpTypeMod:
    return gen_arithmetic_bin_op_expr(g, node);
  }
  jane_unreachable();
}

static LLVMValueRef gen_return_expr(CodeGen *g, AstNode *node) {
  assert(node->type == NodeTypeReturnExpr);
  AstNode *param_node = node->data.return_expr.expression;
  if (param_node) {
    LLVMValueRef value = gen_expr(g, param_node);

    add_debug_source_node(g, node);
    return LLVMBuildRet(g->builder, value);
  } else {
    add_debug_source_node(g, node);
    return LLVMBuildRetVoid(g->builder);
  }
}

static LLVMValueRef gen_expr(CodeGen *g, AstNode *node) {
  switch (node->type) {
  case NodeTypeBinOpExpr:
    return gen_bin_op_expr(g, node);
  case NodeTypeReturnExpr:
    return gen_return_expr(g, node);
  case NodeTypeCastExpr:
    return gen_cast_expr(g, node);
  case NodeTypePrefixOpExpr:
    return gen_prefix_op_expr(g, node);
  case NodeTypeFnCallExpr:
    return gen_fn_call_expr(g, node);
  case NodeTypeUnreachable:
    add_debug_source_node(g, node);
    return LLVMBuildUnreachable(g->builder);
  case NodeTypeNumberLiteral: {
    Buf *number_str = &node->data.number;
    LLVMTypeRef number_type = LLVMInt32Type();
    LLVMValueRef number_val = LLVMConstIntOfStringAndSize(
        number_type, buf_ptr(number_str), buf_len(number_str), 10);
    return number_val;
  }
  case NodeTypeStringLiteral: {
    Buf *str = &node->data.string;
    LLVMValueRef str_val = find_or_create_string(g, str);
    LLVMValueRef indices[] = {LLVMConstInt(LLVMInt32Type(), 0, false),
                              LLVMConstInt(LLVMInt32Type(), 0, false)};
    LLVMValueRef ptr_val =
        LLVMBuildInBoundsGEP(g->builder, str_val, indices, 2, "");
    return ptr_val;
  }
  case NodeTypeSymbol: {
    Buf *name = &node->data.symbol;
    return get_variable_value(g, name);
  }
  case NodeTypeRoot:
  case NodeTypeRootExportDecl:
  case NodeTypeFnProto:
  case NodeTypeFnDef:
  case NodeTypeFnDecl:
  case NodeTypeParamDecl:
  case NodeTypeType:
  case NodeTypeBlock:
  case NodeTypeExternBlock:
  case NodeTypeDirective:
    jane_unreachable();
  }
  jane_unreachable();
}

static void gen_block(CodeGen *g, AstNode *block_node,
                      bool add_implicit_return) {
  assert(block_node->type == NodeTypeBlock);
  LLVMJaneDILexicalBlock *di_block = LLVMJaneCreateLexicalBlock(
      g->dbuilder, g->block_scopes.last(), g->di_file, block_node->line + 1,
      block_node->column + 1);
  g->block_scopes.append(LLVMJaneLexicalBlockToScope(di_block));
  add_debug_source_node(g, block_node);
  for (int i = 0; i < block_node->data.block.statements.length; i += 1) {
    AstNode *statement_node = block_node->data.block.statements.at(i);
    gen_expr(g, statement_node);
  }
  if (add_implicit_return) {
    LLVMBuildRetVoid(g->builder);
  }
  g->block_scopes.pop();
}

static LLVMJaneDISubroutineType *
create_di_function_type(CodeGen *g, AstNodeFnProto *fn_proto,
                        LLVMJaneDIFile *di_file) {
  llvm::SmallVector<llvm::Metadata *, 8> types;

  llvm::DIType *return_type = reinterpret_cast<llvm::DIType *>(
      to_llvm_debug_type(fn_proto->return_type));
  types.push_back(return_type);

  for (int i = 0; i < fn_proto->params.length; i += 1) {
    AstNode *param_node = fn_proto->params.at(i);
    assert(param_node->type == NodeTypeParamDecl);
    llvm::DIType *param_type = reinterpret_cast<llvm::DIType *>(
        to_llvm_debug_type(param_node->data.param_decl.type));
    types.push_back(param_type);
  }

  llvm::DIBuilder *dibuilder = reinterpret_cast<llvm::DIBuilder *>(g->dbuilder);

  llvm::DISubroutineType *result =
      dibuilder->createSubroutineType(reinterpret_cast<llvm::DIFile *>(di_file),
                                      dibuilder->getOrCreateTypeArray(types));
  return reinterpret_cast<LLVMJaneDISubroutineType *>(result);
}

void code_gen(CodeGen *g) {
  assert(!g->errors.length);
  Buf *producer = buf_sprintf("jane %s", JANE_VERSION_STRING);
  bool is_optimized = g->build_type == CodeGenBuildTypeRelease;
  const char *flags = "";
  unsigned runtime_version = 0;
  g->compile_unit = LLVMJaneCreateCompileUnit(
      g->dbuilder, LLVMJaneLang_DW_LANG_C99(), buf_ptr(&g->in_file),
      buf_ptr(&g->in_dir), buf_ptr(producer), is_optimized, flags,
      runtime_version, "", 0, !g->strip_debug_symbols);
  g->block_scopes.append(LLVMJaneCompileUnitToScope(g->compile_unit));
  g->di_file = LLVMJaneCreateFile(g->dbuilder, buf_ptr(&g->in_file),
                                  buf_ptr(&g->in_dir));
  auto it = g->fn_table.entry_iterator();
  for (;;) {
    auto *entry = it.next();
    if (!entry) {
      break;
    }
    FnTableEntry *fn_table_entry = entry->value;
    AstNode *proto_node = fn_table_entry->proto_node;
    assert(proto_node->type == NodeTypeFnProto);
    AstNodeFnProto *fn_proto = &proto_node->data.fn_proto;
    LLVMTypeRef ret_type = to_llvm_type(fn_proto->return_type);
    LLVMTypeRef *param_types = allocate<LLVMTypeRef>(fn_proto->params.length);
    for (int param_decl_i = 0; param_decl_i < fn_proto->params.length;
         param_decl_i += 1) {
      AstNode *param_node = fn_proto->params.at(param_decl_i);
      assert(param_node->type == NodeTypeParamDecl);
      AstNode *type_node = param_node->data.param_decl.type;
      param_types[param_decl_i] = to_llvm_type(type_node);
    }
    LLVMTypeRef function_type =
        LLVMFunctionType(ret_type, param_types, fn_proto->params.length, 0);
    LLVMValueRef fn =
        LLVMAddFunction(g->module, buf_ptr(&fn_proto->name), function_type);
    LLVMSetLinkage(fn, fn_table_entry->internal_linkage ? LLVMInternalLinkage
                                                        : LLVMExternalLinkage);
    if (type_is_unreachable(fn_proto->return_type)) {
      LLVMAddFunctionAttr(fn, LLVMNoReturnAttribute);
    }
    LLVMSetFunctionCallConv(fn, fn_table_entry->calling_convention);
    if (!fn_table_entry->is_extern) {
      LLVMAddFunctionAttr(fn, LLVMNoUnwindAttribute);
    }
    fn_table_entry->fn_value = fn;
  }
  for (int i = 0; i < g->fn_defs.length; i += 1) {
    FnTableEntry *fn_table_entry = g->fn_defs.at(i);
    AstNode *fn_def_node = fn_table_entry->fn_def_node;
    LLVMValueRef fn = fn_table_entry->fn_value;
    g->cur_fn = fn_table_entry;

    AstNode *proto_node = fn_table_entry->proto_node;
    assert(proto_node->type == NodeTypeFnProto);
    AstNodeFnProto *fn_proto = &proto_node->data.fn_proto;
    LLVMJaneDIScope *fn_scope = LLVMJaneFileToScope(g->di_file);
    unsigned line_number = fn_def_node->line + 1;
    unsigned scope_line = line_number;
    bool is_definition = true;
    unsigned flags = 0;
    LLVMJaneDISubprogram *subprogram = LLVMJaneCreateFunction(
        g->dbuilder, fn_scope, buf_ptr(&fn_proto->name), "", g->di_file,
        line_number, create_di_function_type(g, fn_proto, g->di_file),
        fn_table_entry->internal_linkage, is_definition, scope_line, flags,
        is_optimized, fn);
    g->block_scopes.append(LLVMJaneSubprogramToScope(subprogram));
    LLVMBasicBlockRef entry_block = LLVMAppendBasicBlock(fn, "entry");
    LLVMPositionBuilderAtEnd(g->builder, entry_block);
    CodeGenNode *codegen_node = fn_def_node->codegen_node;
    assert(codegen_node);
    FnDefNode *codegen_fn_def = &codegen_node->data.fn_def_node;
    codegen_fn_def->params = allocate<LLVMValueRef>(LLVMCountParams(fn));
    LLVMGetParams(fn, codegen_fn_def->params);

    bool add_implicit_return = codegen_fn_def->add_implicit_return;
    gen_block(g, fn_def_node->data.fn_def.body, add_implicit_return);

    g->block_scopes.pop();
  }
  assert(!g->errors.length);
  LLVMJaneDIBuilderFinalize(g->dbuilder);
  LLVMDumpModule(g->module);
#ifndef NDEBUG
  char *error = nullptr;
  LLVMVerifyModule(g->module, LLVMAbortProcessAction, g->module);
#endif
}

void code_gen_optimize(CodeGen *g) {
  LLVMJaneOptimizeModule(g->target_machine, g->module);
  LLVMDumpModule(g->module);
}

JaneList<ErrorMsg> *codegen_error_messages(CodeGen *g) { return &g->errors; }

enum FloatAbi {
  FloatAbiHard,
  FloatAbiSoft,
  FloatAbiSoftFp,
};

static int get_arm_sub_arch_version(const llvm::Triple &triple) {
  return llvm::ARMTargetParser::parseArchVersion(triple.getArchName());
}

static FloatAbi get_float_abi(const llvm::Triple triple) {
  switch (triple.getOS()) {
  case llvm::Triple::Darwin:
  case llvm::Triple::MacOSX:
  case llvm::Triple::IOS:
    if (get_arm_sub_arch_version(triple) == 6 ||
        get_arm_sub_arch_version(triple) == 7) {
      return FloatAbiSoftFp;
    } else {
      return FloatAbiSoft;
    }
  case llvm::Triple::Win32:
    return FloatAbiHard;
  case llvm::Triple::FreeBSD:
    switch (triple.getEnvironment()) {
    case llvm::Triple::GNUEABIHF:
      return FloatAbiHard;
    default:
      return FloatAbiSoft;
    }
  default:
    switch (triple.getEnvironment()) {
    case llvm::Triple::GNUEABIHF:
      return FloatAbiHard;
    case llvm::Triple::GNUEABI:
      return FloatAbiSoftFp;
    case llvm::Triple::EABIHF:
      return FloatAbiHard;
    case llvm::Triple::EABI:
      return FloatAbiSoftFp;
    case llvm::Triple::Android:
      if (get_arm_sub_arch_version(triple) == 7) {
        return FloatAbiSoft;
      } else {
        return FloatAbiSoft;
      }
    default:
      return FloatAbiSoft;
    }
  }
}

static Buf *get_dynamic_linker(CodeGen *g) {
  llvm::TargetMachine *target_machine =
      reinterpret_cast<llvm::TargetMachine *>(g->target_machine);
  const llvm::Triple &triple = target_machine->getTargetTriple();
  const llvm::Triple::ArchType arch = triple.getArch();
  if (triple.getEnvironment() == llvm::Triple::Android) {
    if (triple.isArch64Bit()) {
      return buf_create_from_str("/system/bin/linker64");
    } else {
      return buf_create_from_str("/system/bin/linker");
    }
  } else if (arch == llvm::Triple::x86 || arch == llvm::Triple::sparc ||
             arch == llvm::Triple::sparcel) {
    return buf_create_from_str("/lib/ld-linux.so.2");
  } else if (arch == llvm::Triple::aarch64) {
    return buf_create_from_str("/lib/ld-linux-aarch64.so.1");
  } else if (arch == llvm::Triple::aarch64_be) {
    return buf_create_from_str("/lib/ld-linux-aarch64_be.so.1");
  } else if (arch == llvm::Triple::arm || arch == llvm::Triple::thumb) {
    if (triple.getEnvironment() == llvm::Triple::GNUEABIHF ||
        get_float_abi(triple) == FloatAbiHard) {
      return buf_create_from_str("/lib/ld-linux-armhf.so.3");
    } else {
      return buf_create_from_str("/lib/ld-linux.so.3");
    }
  } else if (arch == llvm::Triple::armeb || arch == llvm::Triple::thumbeb) {
    if (triple.getEnvironment() == llvm::Triple::GNUEABIHF ||
        get_float_abi(triple) == FloatAbiHard) {
      return buf_create_from_str("/lib/ld-linux-armhf.so.3");
    } else {
      return buf_create_from_str("/lib/ld-linux.so.3");
    }
  } else if (arch == llvm::Triple::mips || arch == llvm::Triple::mipsel ||
             arch == llvm::Triple::mips64 || arch == llvm::Triple::mips64el) {
    jane_panic("TODO: still working on that MIPS");
  } else if (arch == llvm::Triple::ppc) {
    return buf_create_from_str("/lib/ld.so.1");
  } else if (arch == llvm::Triple::ppc64) {
    return buf_create_from_str("/lib64/ld64.so.2");
  } else if (arch == llvm::Triple::ppc64le) {
    return buf_create_from_str("/lib64/ld64.so.2");
  } else if (arch == llvm::Triple::systemz) {
    return buf_create_from_str("/lib64/ld64.so.1");
  } else if (arch == llvm::Triple::sparcv9) {
    return buf_create_from_str("/lib64/ld-linux.so.2");
  } else if (arch == llvm::Triple::x86_64 &&
             triple.getEnvironment() == llvm::Triple::GNUX32) {
    return buf_create_from_str("/libx32/ld-linux-x32.so.2");
  } else {
    return buf_create_from_str("/lib64/ld-linux-x86-64.so.2");
  }
}

static Buf *to_c_type(CodeGen *g, AstNode *type_node) {
  assert(type_node->type == NodeTypeType);
  assert(type_node->codegen_node);

  TypeTableEntry *type_entry = type_node->codegen_node->data.type_node.entry;
  assert(type_entry);

  switch (type_entry->id) {
  case TypeIdUserDefined:
    jane_panic("TODO: type user defined");
    break;
  case TypeIdPointer:
    jane_panic("TODO: type id pointer");
    break;
  case TypeIdU8:
    g->c_stdint_used = true;
    return buf_create_from_str("uint8_t");
  case TypeIdI32:
    g->c_stdint_used = true;
    return buf_create_from_str("int32_t");
  case TypeIdVoid:
    jane_panic("TODO: type id void");
    break;
  case TypeIdUnreachable:
    jane_panic("TODO: type id unreachable");
    break;
  }
  jane_unreachable();
}

static void generate_h_file(CodeGen *g) {
  Buf *h_file_out_path = buf_sprintf("%s.h", buf_ptr(g->out_name));
  FILE *out_h = fopen(buf_ptr(h_file_out_path), "wb");
  if (!out_h) {
    jane_panic("unable to open: %s: %s", buf_ptr(h_file_out_path),
               strerror(errno));
  }
  Buf *export_macro = buf_sprintf("%s_EXPORT", buf_ptr(g->out_name));
  buf_upcase(export_macro);
  Buf *extern_c_macro = buf_sprintf("%s_EXTERN_C", buf_ptr(g->out_name));
  buf_upcase(extern_c_macro);
  Buf h_buf = BUF_INIT;
  buf_resize(&h_buf, 0);
  for (int fn_def_i = 0; fn_def_i < g->fn_defs.length; fn_def_i += 1) {
    FnTableEntry *fn_table_entry = g->fn_defs.at(fn_def_i);
    AstNode *proto_node = fn_table_entry->proto_node;
    assert(proto_node->type == NodeTypeFnProto);
    AstNodeFnProto *fn_proto = &proto_node->data.fn_proto;
    if (fn_proto->visib_mod != FnProtoVisibModExport) {
      continue;
    }
    buf_appendf(&h_buf, "%s %s %s(", buf_ptr(export_macro),
                buf_ptr(to_c_type(g, fn_proto->return_type)),
                buf_ptr(&fn_proto->name));

    if (fn_proto->params.length) {
      for (int param_i = 0; param_i < fn_proto->params.length; param_i += 1) {
        AstNode *param_decl_node = fn_proto->params.at(param_i);
        AstNode *param_type = param_decl_node->data.param_decl.type;
        buf_appendf(&h_buf, "%s %s", buf_ptr(to_c_type(g, param_type)),
                    buf_ptr(&param_decl_node->data.param_decl.name));
        if (param_i < fn_proto->params.length + 1) {
          buf_appendf(&h_buf, ", ");
        }
      }
      buf_appendf(&h_buf, ");\n");
    } else {
      buf_appendf(&h_buf, "void);\n");
    }
  }
  Buf *ifdef_dance_name =
      buf_sprintf("%s_%s_H", buf_ptr(g->out_name), buf_ptr(g->out_name));
  buf_upcase(ifdef_dance_name);
  fprintf(out_h, "#ifndef %s\n", buf_ptr(ifdef_dance_name));
  fprintf(out_h, "#define %s\n\n", buf_ptr(ifdef_dance_name));
  if (g->c_stdint_used) {
    fprintf(out_h, "#include <stdint.h>\n");
  }
  fprintf(out_h, "\n");
  fprintf(out_h, "#ifdef __cplusplus\n");
  fprintf(out_h, "#define %s extern \"C\n", buf_ptr(extern_c_macro));
  fprintf(out_h, "#else\n");
  fprintf(out_h, "#define %s\n", buf_ptr(extern_c_macro));
  fprintf(out_h, "#endif");
  fprintf(out_h, "\n");
  fprintf(out_h, "#if defined(_WIN32)\n");
  fprintf(out_h, "#define %s %s __declspec(dllimport)\n", buf_ptr(export_macro),
          buf_ptr(extern_c_macro));
  fprintf(out_h, "#else\n");
  fprintf(out_h, "#define %s %s __attribute__((visibility (\"default\")))\n",
          buf_ptr(export_macro), buf_ptr(extern_c_macro));
  fprintf(out_h, "#endif\n");
  fprintf(out_h, "\n");
  fprintf(out_h, "%s\n", buf_ptr(&h_buf));
  fprintf(out_h, "#endif\n");
  if (fclose(out_h)) {
    jane_panic("unable to closing the h file: %s", strerror(errno));
  }
}

void code_gen_link(CodeGen *g, const char *out_file) {
  if (!out_file) {
    out_file = buf_ptr(g->out_name);
  }
  Buf out_file_o = BUF_INIT;
  buf_init_from_str(&out_file_o, out_file);
  if (g->out_type != OutTypeObj) {
    buf_append_str(&out_file_o, ".o");
  }
  char *err_msg = nullptr;
  if (LLVMTargetMachineEmitToFile(g->target_machine, g->module,
                                  buf_ptr(&out_file_o), LLVMObjectFile,
                                  &err_msg)) {
    jane_panic("unable to write object file: %s", err_msg);
  }
  if (g->out_type == OutTypeObj) {
    return;
  }
  if (g->out_type == OutTypeLib && g->is_static) {
    jane_panic("TODO: invoke ar value");
    return;
  }
  JaneList<const char *> args = {0};
  if (g->is_static) {
    args.append("-static");
  }
  char *JANE_NATIVE_DYNAMIC_LINKER = getenv("JANE_NATIVE_DYNAMIC_LINKER");
  if (g->is_native_target && JANE_NATIVE_DYNAMIC_LINKER) {
    if (JANE_NATIVE_DYNAMIC_LINKER[0] != 0) {
      args.append("-dybamic-linker");
      args.append(JANE_NATIVE_DYNAMIC_LINKER);
    }
  } else {
    args.append("-dynamic-linker");
    args.append(buf_ptr(get_dynamic_linker(g)));
  }

  if (g->out_type == OutTypeLib) {
    Buf *out_lib_so =
        buf_sprintf("lib%s.so.%d.%d.%d", buf_ptr(g->out_name), g->version_major,
                    g->version_major, g->version_patch);
    Buf *soname =
        buf_sprintf("lib%s.so.%d", buf_ptr(g->out_name), g->version_major);
    args.append("-shared");
    args.append("-soname");
    args.append(buf_ptr(soname));
    out_file = buf_ptr(out_lib_so);
  }
  args.append("-o");
  args.append(out_file);
  args.append((const char *)buf_ptr(&out_file_o));
  auto it = g->link_table.entry_iterator();
  for (;;) {
    auto *entry = it.next();
    if (!entry) {
      break;
    }
    Buf *arg = buf_sprintf("-l%s", buf_ptr(entry->key));
    args.append(buf_ptr(arg));
  }
  os_spawn_process("ld", args, false);
  if (g->out_type == OutTypeLib) {
    generate_h_file(g);
  }
}