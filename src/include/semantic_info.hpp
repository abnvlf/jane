#ifndef JANE_SEMANTIC_INFO
#define JANE_SEMANTIC_INFO

#include "codegen.hpp"
#include "hash_map.hpp"
#include "jane_llvm.hpp"
#include "parser.hpp"

struct FnTableEntry;
struct TypeTableEntry {
  LLVMTypeRef type_ref;
  LLVMJaneDIType *di_type;

  TypeTableEntry *pointer_child;
  bool pointer_is_const;
  int user_defined_id;
  Buf name;
  TypeTableEntry *pointer_const_parent;
  TypeTableEntry *pointer_mut_parent;
};

struct ImportTableEntry {
  AstNode *root;
  Buf *path;
  LLVMJaneDIFile *di_file;
  HashMap<Buf *, FnTableEntry *, buf_hash, buf_eql_buf> fn_table;
};

struct FnTableEntry {
  LLVMValueRef fn_value;
  AstNode *proto_node;
  AstNode *fn_def_node;
  bool is_extern;
  bool internal_linkage;
  unsigned calling_convention;
  ImportTableEntry *import_entry;
};

struct CodeGen {
  LLVMModuleRef module;
  JaneList<ErrorMsg> errors;
  LLVMBuilderRef builder;
  LLVMJaneDIBuilder *dbuilder;
  LLVMJaneDICompileUnit *compile_unit;
  HashMap<Buf *, FnTableEntry *, buf_hash, buf_eql_buf> fn_table;
  HashMap<Buf *, LLVMValueRef, buf_hash, buf_eql_buf> str_table;
  HashMap<Buf *, TypeTableEntry *, buf_hash, buf_eql_buf> type_table;
  HashMap<Buf *, bool, buf_hash, buf_eql_buf> link_table;
  HashMap<Buf *, ImportTableEntry *, buf_hash, buf_eql_buf> import_table;
  struct {
    TypeTableEntry *entry_u8;
    TypeTableEntry *entry_i32;
    TypeTableEntry *entry_void;
    TypeTableEntry *entry_unreachable;
    TypeTableEntry *entry_invalid;
  } builtin_types;

  LLVMTargetDataRef target_data_ref;
  unsigned pointer_size_bytes;
  bool is_static;
  bool strip_debug_symbols;
  CodeGenBuildType build_type;
  LLVMTargetMachineRef target_machine;
  bool is_native_target;
  Buf *root_source_dir;
  Buf *root_out_name;
  JaneList<LLVMJaneDIScope *> block_scopes;
  JaneList<FnTableEntry *> fn_defs;
  OutType out_type;
  FnTableEntry *cur_fn;
  bool c_stdint_used;
  AstNode *root_export_decl;
  int version_major;
  int version_minor;
  int version_patch;
  bool verbose;
};

struct TypeNode {
  TypeTableEntry *entry;
};

struct FnDefNode {
  bool add_implicit_return;
  bool skip;
  LLVMValueRef *params;
};

struct CodeGenNode {
  union {
    TypeNode type_node;
    FnDefNode fn_def_node;
  } data;
};

static inline Buf *hack_get_fn_call_name(CodeGen *g, AstNode *node) {
  assert(node->type == NodeTypeSymbol);
  return &node->data.symbol;
}

#endif // JANE_SEMANTIC_INFO