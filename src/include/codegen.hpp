#ifndef JANE_CODEGEN
#define JANE_CODEGEN

#include "parser.hpp"

struct CodeGen;

enum OutType {
  OutTypeUnknown,
  OutTypeExe,
  OutTypeLib,
  OutTypeObj,
};
struct ErrorMsg {
  int line_start;
  int column_start;
  int line_end;
  int column_end;
  Buf *msg;
};

CodeGen *create_codegen(Buf *root_source_dir);

enum CodeGenBuildType {
  CodeGenBuildTypeDebug,
  CodeGenBuildTypeRelease,
};

void codegen_set_build_type(CodeGen *codegen, CodeGenBuildType build_type);
void codegen_set_is_static(CodeGen *codegen, bool is_static);
void codegen_set_strip(CodeGen *codegen, bool strip);
void codegen_set_verbose(CodeGen *codegen, bool verbose);
void codegen_set_out_type(CodeGen *codegen, OutType out_type);
void codegen_set_out_name(CodeGen *codegen, Buf *out_name);

void codegen_and_code(CodeGen *g, Buf *source_path, Buf *source_code);
void codegen_link(CodeGen *g, const char *out_file);

#endif // JANE_CODEGEN