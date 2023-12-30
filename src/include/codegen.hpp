#ifndef JANE_CODEGEN
#define JANE_CODEGEN

#include "parser.hpp"

struct CodeGen;
struct ErrorMsg {
  int line_start;
  int column_start;
  int line_end;
  int column_end;
  Buf *msg;
};

CodeGen *create_codegen(AstNode *root, Buf *in_files);
enum CodeGenBuildType {
  CodeGenBuildTypeDebug,
  CodeGenBuildTypeRelease,
};

void codegen_set_build_type(CodeGen *codegen, CodeGenBuildType build_type);
void codegen_set_is_static(CodeGen *codegen, bool is_static);
void codegen_set_strip(CodeGen *codegen, bool strip);
void semantic_analyze(CodeGen *g);
void code_gen(CodeGen *g);
void code_gen_link(CodeGen *g, const char *out_file);
JaneList<ErrorMsg> *codegen_error_message(CodeGen *g);

#endif // JANE_CODEGEN