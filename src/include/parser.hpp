#ifndef JANE_PARSER
#define JANE_PARSER

#include "buffer.hpp"
#include "list.hpp"
#include "tokenizer.hpp"

struct AstNode;
struct CodeGenNode;

enum NodeType {
  NodeTypeRoot,
  NodeTypeFnProto,
  NodeTypeFnDef,
  NodeTypeFnDecl,
  NodeTypeParamDecl,
  NodeTypeType,
  NodeTypeBlock,
  NodeTypeExpression,
  NodeTypeFnCall,
  NodeTypeExternBlock,
  NodeTypeDirective,
  NodeTypeStatementReturn,
};

struct AstNodeRoot {
  JaneList<AstNode *> top_level_decls;
};

struct AstNodeFnProto {
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
  AstNode *body;
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

struct AstNodeStatementReturn {
  AstNode *expression;
};

enum AstNodeExpressionType {
  AstNodeExpressionTypeNumber,
  AstNodeExpressionTypeString,
  AstNodeExpressionTypeFnCall,
  AstNodeExpressionTypeUnreachable,
};

struct AstNodeExpression {
  AstNodeExpressionType type;
  union {
    Buf number;
    Buf string;
    AstNode *fn_call;
  } data;
};

struct AstNodeFnCall {
  Buf name;
  JaneList<AstNode *> params;
};

struct AstNodeExternBlock {
  JaneList<AstNode *> *directive;
  JaneList<AstNode *> fn_decls;
};

struct AstNodeDirective {
  Buf name;
  Buf param;
};

struct AstNode {
  enum NodeType type;
  AstNode *parent;
  int line;
  int column;
  CodeGenNode *codegen_node;
  union {
    AstNodeRoot root;
    AstNodeFnDef fn_def;
    AstNodeFnDecl fn_decl;
    AstNodeFnProto fn_proto;
    AstNodeType type;
    AstNodeParamDecl param_decl;
    AstNodeBlock block;
    AstNodeStatementReturn statement_return;
    AstNodeExpression expression;
    AstNodeFnCall fn_call;
    AstNodeExternBlock extern_block;
    AstNodeDirective directive;
  } data;
};

__attribute__((format(printf, 2, 3))) void
ast_token_error(Token *token, const char *format, ...);

AstNode *ast_parse(Buf *buf, JaneList<Token> *tokens);
const char *node_type_str(NodeType node_type);
void ast_print(AstNode *node, int indent);

#endif // JANE_PARSER