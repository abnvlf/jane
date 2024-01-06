#ifndef JANE_PARSER
#define JANE_PARSER

#include "buffer.hpp"
#include "list.hpp"
#include "tokenizer.hpp"

struct AstNode;
struct CodeGenNode;

enum NodeType {
  NodeTypeRoot,
  NodeTypeRootExportDecl,
  NodeTypeFnProto,
  NodeTypeFnDef,
  NodeTypeFnDecl,
  NodeTypeParamDecl,
  NodeTypeType,
  NodeTypeBlock,
  NodeTypeExternBlock,
  NodeTypeDirective,
  NodeTypeReturnExpr,
  NodeTypeBinOpExpr,
  NodeTypeCastExpr,
  NodeTypeNumberLiteral,
  NodeTypeStringLiteral,
  NodeTypeUnreachable,
  NodeTypeSymbol,
  NodeTypePrefixOpExpr,
  NodeTypeFnCallExpr,
  NodeTypeUse,
};

struct AstNodeRoot {
  JaneList<AstNode *> top_level_decls;
};

enum FnProtoVisibMod {
  FnProtoVisibModPrivate,
  FnProtoVisibModPub,
  FnProtoVisibModExport,
};

struct AstNodeFnProto {
  JaneList<AstNode *> *directives;
  FnProtoVisibMod visib_mod;
  Buf name;
  JaneList<AstNode *> params;
  AstNode *return_type;
};

struct AstNodeFnDef {
  AstNode *fn_proto;
  AstNode *body;
};

struct AstNodeFnDecl {
  AstNode *fn_proto;
};

struct AstNodeParamDecl {
  Buf name;
  AstNode *type;
};

enum AstNodeTypeType {
  AstNodeTypeTypePrimitive,
  AstNodeTypeTypePointer,
};

struct AstNodeType {
  AstNodeTypeType type;
  Buf primitive_name;
  AstNode *child_type;
  bool is_const;
};
struct AstNodeBlock {
  JaneList<AstNode *> statements;
};

struct AstNodeReturnExpr {
  AstNode *expression;
};

enum BinOpType {
  BinOpTypeInvalid,
  BinOpTypeBoolOr,
  BinOpTypeBoolAnd,
  BinOpTypeCmpEq,
  BinOpTypeCmpNotEq,
  BinOpTypeCmpLessThan,
  BinOpTypeCmpGreaterThan,
  BinOpTypeCmpLessOrEq,
  BinOpTypeCmpGreaterOrEq,
  BinOpTypeBinOr,
  BinOpTypeBinXor,
  BinOpTypeBinAnd,
  BinOpTypeBitShiftLeft,
  BinOpTypeBitShiftRight,
  BinOpTypeAdd,
  BinOpTypeSub,
  BinOpTypeMult,
  BinOpTypeDiv,
  BinOpTypeMod,
};

struct AstNodeBinOpExpr {
  AstNode *op1;
  BinOpType bin_op;
  AstNode *op2;
};

struct AstNodeFnCallExpr {
  AstNode *fn_ref_expr;
  JaneList<AstNode *> params;
};

struct AstNodeExternBlock {
  JaneList<AstNode *> *directives;
  JaneList<AstNode *> fn_decls;
};

struct AstNodeDirective {
  Buf name;
  Buf param;
};

struct AstNodeRootExportDecl {
  Buf type;
  Buf name;
  JaneList<AstNode *> *directives;
};

struct AstNodeCastExpr {
  AstNode *prefix_op_expr;
  AstNode *type;
};

enum PrefixOp {
  PrefixOpInvalid,
  PrefixOpBoolNot,
  PrefixOpBinNot,
  PrefixOpNegation,
};

struct AstNodePrefixOpExpr {
  PrefixOp prefix_op;
  AstNode *primary_expr;
};

struct AstNodeUse {
  Buf path;
  JaneList<AstNode *> *directive;
};

struct AstNode {
  enum NodeType type;
  AstNode *parent;
  int line;
  int column;
  CodeGenNode *codegen_node;
  union {
    AstNodeRoot root;
    AstNodeRootExportDecl root_export_decl;
    AstNodeFnDef fn_def;
    AstNodeFnDecl fn_decl;
    AstNodeFnProto fn_proto;
    AstNodeType type;
    AstNodeParamDecl param_decl;
    AstNodeBlock block;
    AstNodeReturnExpr return_expr;
    AstNodeBinOpExpr bin_op_expr;
    AstNodeExternBlock extern_block;
    AstNodeDirective directive;
    AstNodeCastExpr cast_expr;
    AstNodePrefixOpExpr prefix_op_expr;
    AstNodeFnCallExpr fn_call_expr;
    AstNodeUse use;
    Buf number;
    Buf string;
    Buf symbol;
  } data;
};
__attribute__((format(printf, 2, 3))) void
ast_token_error(Token *token, const char *format, ...);

AstNode *ast_parse(Buf *buf, JaneList<Token> *tokens);
const char *node_type_str(NodeType node_type);
void ast_print(AstNode *node, int indent);

#endif // JANE_PARSER