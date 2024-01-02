#include "include/analyze.hpp"
#include "include/buffer.hpp"
#include "include/codegen.hpp"
#include "include/error.hpp"
#include "include/jane_llvm.hpp"
#include "include/parser.hpp"
#include "include/semantic_info.hpp"
#include "include/util.hpp"
#include <llvm-c/Core.h>
#include <llvm-c/TargetMachine.h>

static void add_node_error(CodeGen *g, AstNode *node, Buf *msg) {
  g->errors.add_one();
  ErrorMsg *last_msg = &g->errors.last();
  last_msg->line_start = node->line;
  last_msg->column_start = node->column;
  last_msg->line_end = -1;
  last_msg->column_end = -1;
  last_msg->msg = msg;
}

static int parse_version_string(Buf *buf, int *major, int *minor, int *patch) {
  char *dot1 = strstr(buf_ptr(buf), ".");
  if (!dot1) {
    return ErrorInvalidFormat;
  }
  char *dot2 = strstr(dot1 + 1, ".");
  if (!dot2) {
    return ErrorInvalidFormat;
  }
  *major = (int)strtol(buf_ptr(buf), nullptr, 10);
  *minor = (int)strtol(dot1 + 1, nullptr, 10);
  *patch = (int)strtol(dot2 + 1, nullptr, 10);
  return ErrorNone;
}

static void set_root_export_version(CodeGen *g, Buf *version_buf,
                                    AstNode *node) {
  int err;
  if ((err = parse_version_string(version_buf, &g->version_major,
                                  &g->version_minor, &g->version_patch))) {
    add_node_error(g, node, buf_sprintf("invalid version_string"));
  }
}

static void resolve_type(CodeGen *g, AstNode *node) {
  assert(!node->codegen_node);
  node->codegen_node = allocate<CodeGenNode>(1);
  TypeNode *type_node = &node->codegen_node->data.type_node;
  switch (node->data.type.type) {
  case AstNodeTypeTypePrimitive: {
    Buf *name = &node->data.type.primitive_name;
    auto table_entry = g->type_table.maybe_get(name);
    if (table_entry) {
      type_node->entry = table_entry->value;
    } else {
      add_node_error(g, node,
                     buf_sprintf("invalid type name: `%s`", buf_ptr(name)));
      type_node->entry = g->invalid_type_entry;
    }
    break;
  }
  case AstNodeTypeTypePointer: {
    resolve_type(g, node->data.type.child_type);
    TypeNode *child_type_node =
        &node->data.type.child_type->codegen_node->data.type_node;
    if (child_type_node->entry->id == TypeIdUnreachable) {
      add_node_error(g, node,
                     buf_create_from_str("pointer to unreachable not allowed"));
    }
    TypeTableEntry **parent_pointer =
        node->data.type.is_const ? &child_type_node->entry->pointer_const_parent
                                 : &child_type_node->entry->pointer_mut_parent;
    const char *const_or_mut_str = node->data.type.is_const ? "const" : "mut";
    if (*parent_pointer) {
      type_node->entry = *parent_pointer;
    } else {
      TypeTableEntry *entry = allocate<TypeTableEntry>(1);
      entry->id = TypeIdPointer;
      entry->type_ref = LLVMPointerType(child_type_node->entry->type_ref, 0);
      buf_resize(&entry->name, 0);
      buf_appendf(&entry->name, "*%s %s", const_or_mut_str,
                  buf_ptr(&child_type_node->entry->name));
      entry->di_type = LLVMJaneCreateDebugPointerType(
          g->dbuilder, child_type_node->entry->di_type,
          g->pointer_size_bytes * 8, g->pointer_size_bytes * 8,
          buf_ptr(&entry->name));
      g->type_table.put(&entry->name, entry);
      type_node->entry = entry;
      *parent_pointer = entry;
    }
    break;
  }
  }
}

static void resolve_function_proto(CodeGen *g, AstNode *node) {
  assert(node->type == NodeTypeFnProto);
  for (int i = 0; i < node->data.fn_proto.directives->length; i += 1) {
    AstNode *directive_node = node->data.fn_proto.directives->at(i);
    Buf *name = &directive_node->data.directive.name;
    add_node_error(g, directive_node,
                   buf_sprintf("invalid directive: `%s`", buf_ptr(name)));
  }
  for (int i = 0; i < node->data.fn_proto.params.length; i += 1) {
    AstNode *child = node->data.fn_proto.params.at(i);
    assert(child->type == NodeTypeParamDecl);
    resolve_type(g, child->data.param_decl.type);
  }
  resolve_type(g, node->data.fn_proto.return_type);
}

static void preview_function_declarations(CodeGen *g, AstNode *node) {
  switch (node->type) {
  case NodeTypeExternBlock:
    for (int i = 0; i < node->data.extern_block.directives->length; i += 1) {
      AstNode *directive_node = node->data.extern_block.directives->at(i);
      Buf *name = &directive_node->data.directive.name;
      Buf *param = &directive_node->data.directive.param;
      if (buf_eql_str(name, "link")) {
        g->link_table.put(param, true);
      } else {
        add_node_error(g, directive_node,
                       buf_sprintf("invalid directive: `%s`", buf_ptr(name)));
      }
    }
    for (int fn_decl_i = 0; fn_decl_i < node->data.extern_block.fn_decls.length;
         fn_decl_i += 1) {
      AstNode *fn_decl = node->data.extern_block.fn_decls.at(fn_decl_i);
      assert(fn_decl->type == NodeTypeFnDecl);
      AstNode *fn_proto = fn_decl->data.fn_decl.fn_proto;
      resolve_function_proto(g, fn_proto);
      Buf *name = &fn_proto->data.fn_proto.name;

      FnTableEntry *fn_table_entry = allocate<FnTableEntry>(1);
      fn_table_entry->proto_node = fn_proto;
      fn_table_entry->is_extern = true;
      fn_table_entry->calling_convention = LLVMCCallConv;
      g->fn_table.put(name, fn_table_entry);
    }
    break;
  case NodeTypeFnDef: {
    AstNode *proto_node = node->data.fn_def.fn_proto;
    assert(proto_node->type == NodeTypeFnProto);
    Buf *proto_name = &proto_node->data.fn_proto.name;
    auto entry = g->fn_table.maybe_get(proto_name);
    if (entry) {
      add_node_error(g, node,
                     buf_sprintf("redifinition of `%s`", buf_ptr(proto_name)));
      assert(!node->codegen_node);
      node->codegen_node = allocate<CodeGenNode>(1);
      node->codegen_node->data.fn_def_node.skip = true;
    } else {
      FnTableEntry *fn_table_entry = allocate<FnTableEntry>(1);
      fn_table_entry->proto_node = proto_node;
      fn_table_entry->fn_def_node = node;
      fn_table_entry->internal_linkage =
          proto_node->data.fn_proto.visib_mod != FnProtoVisibModExport;
      if (fn_table_entry->internal_linkage) {
        fn_table_entry->calling_convention = LLVMFastCallConv;
      } else {
        fn_table_entry->calling_convention = LLVMCCallConv;
      }
      g->fn_table.put(proto_name, fn_table_entry);
      g->fn_defs.append(fn_table_entry);
      resolve_function_proto(g, proto_node);
    }
  } break;
  case NodeTypeRootExportDecl:
    for (int i = 0; i < node->data.root_export_decl.directives->length;
         i += 1) {
      AstNode *directive_node = node->data.root_export_decl.directives->at(i);
      Buf *name = &directive_node->data.directive.name;
      Buf *param = &directive_node->data.directive.param;
      if (buf_eql_str(name, "version")) {
        set_root_export_version(g, param, directive_node);
      } else {
        add_node_error(g, directive_node,
                       buf_sprintf("invalid directive: `%s`", buf_ptr(name)));
      }
    }
    if (g->root_export_decl) {
      add_node_error(g, node,
                     buf_sprintf("only one root export declaration allowed"));
    } else {
      g->root_export_decl = node;
      if (!g->out_name) {
        g->out_name = &node->data.root_export_decl.name;
      }
      Buf *out_type = &node->data.root_export_decl.type;
      OutType export_out_type;
      if (buf_eql_str(out_type, "exectuable")) {
        export_out_type = OutTypeExe;
      } else if (buf_eql_str(out_type, "library")) {
        export_out_type = OutTypeLib;
      } else if (buf_eql_str(out_type, "boject")) {
        export_out_type = OutTypeObj;
      } else {
        add_node_error(
            g, node,
            buf_sprintf("invalid export type: `%s`", buf_ptr(out_type)));
      }
      if (g->out_type == OutTypeUnknown) {
        g->out_type = export_out_type;
      }
    }
    break;
  case NodeTypeDirective:
  case NodeTypeParamDecl:
  case NodeTypeFnProto:
  case NodeTypeType:
  case NodeTypeFnDecl:
  case NodeTypeReturnExpr:
  case NodeTypeRoot:
  case NodeTypeBlock:
  case NodeTypeBinOpExpr:
  case NodeTypeFnCallExpr:
  case NodeTypeNumberLiteral:
  case NodeTypeStringLiteral:
  case NodeTypeUnreachable:
  case NodeTypeSymbol:
  case NodeTypeCastExpr:
  case NodeTypePrefixOpExpr:
    jane_unreachable();
  }
}

static void check_fn_def_control_flow(CodeGen *g, AstNode *node) {
  assert(node->type == NodeTypeFnDef);
  AstNode *proto_node = node->data.fn_def.fn_proto;
  assert(proto_node->type == NodeTypeFnProto);
  AstNode *return_type_node = proto_node->data.fn_proto.return_type;
  assert(return_type_node->type == NodeTypeType);
  node->codegen_node = allocate<CodeGenNode>(1);
  FnDefNode *codegen_fn_def = &node->codegen_node->data.fn_def_node;
  assert(return_type_node->codegen_node);
  TypeTableEntry *type_entry =
      return_type_node->codegen_node->data.type_node.entry;
  assert(type_entry);
  TypeId type_id = type_entry->id;
  AstNode *body_node = node->data.fn_def.body;
  assert(body_node->type == NodeTypeBlock);

  bool prev_statement_return = false;
  for (int i = 0; i < body_node->data.block.statements.length; i += 1) {
    AstNode *statement_node = body_node->data.block.statements.at(i);
    if (statement_node->type == NodeTypeReturnExpr) {
      if (type_id == TypeIdUnreachable) {
        add_node_error(
            g, statement_node,
            buf_sprintf(
                "return statement function with unreachable return type"));
        return;
      } else {
        prev_statement_return = true;
      }
    } else if (prev_statement_return) {
      add_node_error(g, statement_node, buf_sprintf("unreachable code"));
    }
  }

  if (!prev_statement_return) {
    if (type_id == TypeIdVoid) {
      codegen_fn_def->add_implicit_return = true;
    } else if (type_id != TypeIdUnreachable) {
      add_node_error(g, node,
                     buf_sprintf("control reaches end of non-void function"));
    }
  }
}

static void analyze_expression(CodeGen *g, AstNode *node) {
  switch (node->type) {
  case NodeTypeBlock:
    for (int i = 0; i < node->data.block.statements.length; i += 1) {
      AstNode *child = node->data.block.statements.at(i);
      analyze_expression(g, child);
    }
    break;
  case NodeTypeReturnExpr:
    if (node->data.return_expr.expression) {
      analyze_expression(g, node->data.return_expr.expression);
    }
    break;
  case NodeTypeBinOpExpr:
    analyze_expression(g, node->data.bin_op_expr.op1);
    analyze_expression(g, node->data.bin_op_expr.op2);
    break;
  case NodeTypeFnCallExpr: {
    Buf *name = hack_get_fn_call_name(g, node->data.fn_call_expr.fn_ref_expr);
    auto entry = g->fn_table.maybe_get(name);
    if (!entry) {
      add_node_error(g, node,
                     buf_sprintf("undefined function: `%s`", buf_ptr(name)));
    } else {
      FnTableEntry *fn_table_entry = entry->value;
      assert(fn_table_entry->proto_node->type == NodeTypeFnProto);
      int expected_param_count =
          fn_table_entry->proto_node->data.fn_proto.params.length;
      int actual_param_count = node->data.fn_call_expr.params.length;
      if (expected_param_count != actual_param_count) {
        add_node_error(
            g, node,
            buf_sprintf("wrong number of argument, expect %d, got a %d",
                        expected_param_count, actual_param_count));
      }
    }
    for (int i = 0; i < node->data.fn_call_expr.params.length; i += 1) {
      AstNode *child = node->data.fn_call_expr.params.at(i);
      analyze_expression(g, child);
    }
    break;
  }
  case NodeTypeCastExpr:
    jane_panic("TODO: node type cast expr");
    break;
  case NodeTypePrefixOpExpr:
    jane_panic("TODO: node type prefix operator expression");
    break;
  case NodeTypeNumberLiteral:
  case NodeTypeStringLiteral:
  case NodeTypeUnreachable:
  case NodeTypeSymbol:
    break;
  case NodeTypeDirective:
  case NodeTypeFnDecl:
  case NodeTypeFnProto:
  case NodeTypeParamDecl:
  case NodeTypeType:
  case NodeTypeRoot:
  case NodeTypeRootExportDecl:
  case NodeTypeExternBlock:
  case NodeTypeFnDef:
    jane_unreachable();
  }
}

static void analyze_top_level_declaration(CodeGen *g, AstNode *node) {
  switch (node->type) {
  case NodeTypeFnDef: {
    if (node->codegen_node && node->codegen_node->data.fn_def_node.skip) {
      break;
    }
    AstNode *fn_proto_node = node->data.fn_def.fn_proto;
    assert(fn_proto_node->type == NodeTypeFnProto);
    AstNodeFnProto *fn_proto = &fn_proto_node->data.fn_proto;
    for (int i = 0; i < fn_proto->params.length; i += 1) {
      AstNode *param_decl_node = fn_proto->params.at(i);
      assert(param_decl_node->type == NodeTypeParamDecl);
    }
    check_fn_def_control_flow(g, node);
    analyze_expression(g, node->data.fn_def.body);
  } break;
  case NodeTypeRootExportDecl:
  case NodeTypeExternBlock:
    break;
  case NodeTypeDirective:
  case NodeTypeParamDecl:
  case NodeTypeFnProto:
  case NodeTypeType:
  case NodeTypeFnDecl:
  case NodeTypeReturnExpr:
  case NodeTypeRoot:
  case NodeTypeBlock:
  case NodeTypeBinOpExpr:
  case NodeTypeFnCallExpr:
  case NodeTypeNumberLiteral:
  case NodeTypeStringLiteral:
  case NodeTypeUnreachable:
  case NodeTypeSymbol:
  case NodeTypeCastExpr:
  case NodeTypePrefixOpExpr:
    jane_unreachable();
  }
}

static void analyze_root(CodeGen *g, AstNode *node) {
  assert(node->type == NodeTypeRoot);
  for (int i = 0; i < node->data.root.top_level_decls.length; i += 1) {
    AstNode *child = node->data.root.top_level_decls.at(i);
    preview_function_declarations(g, child);
  }

  for (int i = 0; i < node->data.root.top_level_decls.length; i += 1) {
    AstNode *child = node->data.root.top_level_decls.at(i);
    analyze_top_level_declaration(g, child);
  }

  if (!g->out_name) {
    add_node_error(
        g, node,
        buf_sprintf("missing export declaration and outptu name not provided"));
  } else if (g->out_type == OutTypeUnknown) {
    add_node_error(
        g, node,
        buf_sprintf("missing export declaration and export type not provided"));
  }
}

static void define_primitive_types(CodeGen *g) {
  {
    TypeTableEntry *entry = allocate<TypeTableEntry>(1);
    entry->id = TypeIdU8;
    entry->type_ref = LLVMInt8Type();
    buf_init_from_str(&entry->name, "u8");
    entry->di_type =
        LLVMJaneCreateDebugBasicType(g->dbuilder, buf_ptr(&entry->name), 8, 8,
                                     LLVMJaneEncoding_DW_ATE_unsigned());
    g->type_table.put(&entry->name, entry);
  }
  {
    TypeTableEntry *entry = allocate<TypeTableEntry>(1);
    entry->id = TypeIdI32;
    entry->type_ref = LLVMInt32Type();
    buf_init_from_str(&entry->name, "i32");
    entry->di_type =
        LLVMJaneCreateDebugBasicType(g->dbuilder, buf_ptr(&entry->name), 32, 32,
                                     LLVMJaneEncoding_DW_ATE_signed());
    g->type_table.put(&entry->name, entry);
  }
  {
    TypeTableEntry *entry = allocate<TypeTableEntry>(1);
    entry->id = TypeIdVoid;
    entry->type_ref = LLVMVoidType();
    buf_init_from_str(&entry->name, "void");
    entry->di_type =
        LLVMJaneCreateDebugBasicType(g->dbuilder, buf_ptr(&entry->name), 0, 0,
                                     LLVMJaneEncoding_DW_ATE_unsigned());
    g->type_table.put(&entry->name, entry);

    g->invalid_type_entry = entry;
  }
  {
    TypeTableEntry *entry = allocate<TypeTableEntry>(1);
    entry->id = TypeIdUnreachable;
    entry->type_ref = LLVMVoidType();
    buf_init_from_str(&entry->name, "unreachable");
    entry->di_type = g->invalid_type_entry->di_type;
    g->type_table.put(&entry->name, entry);
  }
}

void semantic_analyze(CodeGen *g) {
  LLVMInitializeAllTargets();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllAsmPrinters();
  LLVMInitializeAllAsmParsers();
  LLVMInitializeNativeTarget();

  g->is_native_target = true;
  char *native_triple = LLVMGetDefaultTargetTriple();

  LLVMTargetRef target_ref;
  char *err_msg = nullptr;
  if (LLVMGetTargetFromTriple(native_triple, &target_ref, &err_msg)) {
    jane_panic("unable to get target from triple: %s", err_msg);
  }

  char *native_cpu = LLVMJaneGetHostCPUName();
  char *native_features = LLVMJaneGetNativeFeatures();

  LLVMCodeGenOptLevel opt_level = (g->build_type == CodeGenBuildTypeDebug)
                                      ? LLVMCodeGenLevelNone
                                      : LLVMCodeGenLevelAggressive;

  LLVMRelocMode reloc_mode = g->is_static ? LLVMRelocStatic : LLVMRelocPIC;
  g->target_machine = LLVMCreateTargetMachine(
      target_ref, native_triple, native_cpu, native_features, opt_level,
      reloc_mode, LLVMCodeModelDefault);
  g->module = LLVMModuleCreateWithName("JaneModule");
  g->pointer_size_bytes = LLVMPointerSize(g->target_data_ref);
  g->builder = LLVMCreateBuilder();
  g->dbuilder = LLVMJaneCreateDIBuilder(g->module, true);
  define_primitive_types(g);
  analyze_root(g, g->root);
}