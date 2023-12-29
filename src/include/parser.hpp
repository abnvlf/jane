#ifndef JANE_PARSER
#define JANE_PARSER

#include "buffer.hpp"
#include "list.hpp"
#include "tokenizer.hpp"

struct AstNode;
enum NodeType {
  NodeTypeRoot,
  NodeTypeFnDecl,
  NodeTypeParamDecl,
  NodeTypeType,
  NodeTypePointerType,
  NodeTypeBlock,
  NodeTypeStatement,
  NodeTypeExpressionStatement,
  NodeTypeReturnStatement,
  NodeTypeExpression,
  NodeTypeFnCall,
};

struct AstNodeRoot {
  JaneList<AstNode *> fn_decls;
};

struct AstNodeFnDecl {
  Buf name;
  JaneList<AstNode *> params;
  AstNode *return_type;
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
  AstNode *child;
};

struct AstNodePointerType {
  AstNode *const_or_mut;
  AstNode *type;
};

struct AstNodeBlock {
  JaneList<AstNode *> expression;
};

struct AstNodeExpression {
  AstNode *child;
};

struct AstNodeFnCall {
  Buf name;
  JaneList<AstNode *> params;
};

struct AstNode {
  enum NodeType type;
  AstNode *parent;
  union {
    AstNodeRoot root;
    AstNodeFnDecl fn_decl;
    AstNodeType type;
    AstNodeParamDecl param_decl;
    AstNodeBlock block;
    AstNodeExpression expression;
    AstNodeFnCall fn_call;
  } data;
};

__attribute__((format(printf, 2, 3))) void ast_error(Token *token,
                                                     const char *format, ...);
AstNode *ast_parse(Buf *buf, JaneList<Token> *tokens);
const char *node_type_str(NodeType node_type);
void ast_print(AstNode *node, int indent);

#endif // JANE_PARSER