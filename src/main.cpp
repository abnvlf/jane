#include "../config.h"
#include "include/buffer.hpp"
#include "include/codegen.hpp"
#include "include/error.hpp"
#include "include/list.hpp"
#include "include/parser.hpp"
#include "include/tokenizer.hpp"
#include "include/util.hpp"

#include <cstdlib>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int usage(const char *arg0) {
  fprintf(stderr,
          "usage: %s (command) (option) target\n"
          "command\n"
          "build    [create an executable from target files]\n"
          "link     [turn `.o` file to executable files]\n"
          "option\n"
          "--output   [output file]\n"
          "--version  [display version of jane language]\n"
          "--release  [build with optimization on]\n"
          "--strip    [exclude debug symbol]\n"
          "--static   [build a static executable]\n"
          "-Ipath     [add path to haeder include path]\n",
          arg0);
  return EXIT_FAILURE;
}

static Buf *fetch_file(FILE *f) {
  int fd = fileno(f);
  struct stat st;
  if (fstat(fd, &st)) {
    jane_panic("unable to stat file: %s", strerror(errno));
  }
  off_t big_size = st.st_size;
  if (big_size > INT_MAX) {
    jane_panic("file to big");
  }
  int size = (int)big_size;
  Buf *buf = buf_alloc_fixed(size);
  size_t amt_read = fread(buf_ptr(buf), 1, buf_len(buf), f);
  if (amt_read != (size_t)buf_len(buf)) {
    jane_panic("error reading: `%s`", strerror(errno));
  }
  return buf;
}

static int build(const char *arg0, const char *in_files, const char *out_files,
                 JaneList<char *> *include_paths, bool release, bool strip,
                 bool is_static) {
  static char cur_dir[1024];
  if (!in_files || !out_files) {
    return usage(arg0);
  }

  FILE *in_f;
  if (strcmp(in_files, "-") == 0) {
    in_f = stdin;
    char *result = getcwd(cur_dir, sizeof(cur_dir));
    if (!result) {
      jane_panic("unable get current work directory: %s", strerror(errno));
    }
  } else {
    in_f = fopen(in_files, "rb");
    if (!in_f) {
      jane_panic("unable to open %s for reading: %s\n", in_files,
                 strerror(errno));
    }
  }
  fprintf(stderr, "original source code:\n");
  fprintf(stderr, "----\n");
  Buf *in_data = fetch_file(in_f);
  fprintf(stderr, "%s\n", buf_ptr(in_data));

  fprintf(stderr, "\ntoken:\n");
  fprintf(stderr, "----\n");
  JaneList<Token> *tokens = tokenize(in_data);
  print_tokens(in_data, tokens);

  fprintf(stderr, "\nAST:\n");
  fprintf(stderr, "----\n");
  AstNode *root = ast_parse(in_data, tokens);
  assert(root);
  ast_print(root, 0);

  fprintf(stderr, "\nsemantic analysis\n");
  fprintf(stderr, "----\n");
  CodeGen *codegen = create_codegen(root, buf_create_from_str(in_files));
  codegen_set_build_type(codegen, release ? CodeGenBuildTypeRelease
                                          : CodeGenBuildTypeDebug);
  codegen_set_strip(codegen, strip);
  codegen_set_is_static(codegen, is_static);
  semantic_analyze(codegen);
  JaneList<ErrorMsg> *errors = codegen_error_message(codegen);
  if (errors->length == 0) {
    fprintf(stderr, "nice one\n");
  } else {
    for (int i = 0; i < errors->length; i += 1) {
      ErrorMsg *err = &errors->at(i);
      fprintf(stderr, "UPSS ERROR: line `%d`, column `%d`: `%s`\n",
              err->line_start + 1, err->column_start + 1, buf_ptr(err->msg));
    }
    return 1;
  }
  fprintf(stderr, "\ncode generate:\n");
  fprintf(stderr, "---\n");
  code_gen(codegen);

  fprintf(stderr, "\nlink:\n");
  fprintf(stderr, "---\n");
  code_gen_link(codegen, out_files);
  fprintf(stderr, "nice one\n");
  return 0;
}

enum Cmd {
  CmdNone,
  CmdBuild,
};

int main(int argc, char **argv) {
  char *arg0 = argv[0];
  char *in_file = NULL;
  char *out_file = NULL;
  JaneList<char *> include_paths = {0};
  bool release = false;
  bool strip = false;
  bool is_static = false;

  Cmd cmd = CmdNone;
  for (int i = 1; i < argc; i += 1) {
    char *arg = argv[i];
    if (arg[0] == '-' && arg[1] == '-') {
      if (strcmp(arg, "--version") == 0) {
        printf("jane v%s\n", JANE_VERSION_STRING);
        return EXIT_SUCCESS;
      } else if (strcmp(arg, "--release") == 0) {
        release = true;
      } else if (strcmp(arg, "--strip") == 0) {
        strip = true;
      } else if (strcmp(arg, "--static") == 0) {
        is_static = true;
      } else if (i + 1 >= argc) {
        return usage(arg0);
      } else {
        i += 1;
        if (strcmp(arg, "--output") == 0) {
          out_file = argv[i];
        } else {
          return usage(arg0);
        }
      }
    } else if (arg[0] == '-' && arg[1] == 'I') {
      include_paths.append(arg + 2);
    } else if (cmd == CmdNone) {
      if (strcmp(arg, "build") == 0) {
        cmd = CmdBuild;
      } else {
        fprintf(stderr, "Unrecognized command: %s\n", arg);
        return usage(arg0);
      }
    } else {
      switch (cmd) {
      case CmdNone:
        jane_unreachable();
      case CmdBuild:
        if (!in_file) {
          in_file = arg;
        } else {
          return usage(arg0);
        }
        break;
      }
    }
  }

  switch (cmd) {
  case CmdNone:
    return usage(arg0);
  case CmdBuild:
    return build(arg0, in_file, out_file, &include_paths, release, strip,
                 is_static);
  }

  jane_unreachable();
}
