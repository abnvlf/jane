#include "../config.h"
#include "include/buffer.hpp"
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

static int usage

    (char *arg0) {
  fprintf(stderr,
          "usage: %s --output outfile file.jn\nOther:\n--version (display "
          "version jane)\n-Ipath (add path to header include path)\n",
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

void ast_error(Token *token, const char *format, ...) {
  int line = token->start_line = 1;
  int column = token->start_column + 1;
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "Error: Line %d, column %d: ", line, column);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  exit(EXIT_FAILURE);
}

static const char *node_type_str(NodeType node_type) {
  switch (node_type) {
  case NodeTypeRoot:
    return "Root";
  case NodeTypeFnDecl:
    return "FnDecl";
  case NodeTypeParamDecl:
    return "ParamDecl";
  case NodeTypeType:
    return "Type";
  case NodeTypePointerType:
    return "PointerType";
  case NodeTypeBlock:
    return "Block";
  case NodeTypeStatement:
    return "Statement";
  case NodeTypeExpressionStatement:
    return "ExpressionStatement";
  case NodeTypeReturnStatement:
    return "ReturnStatement";
  case NodeTypeExpression:
    return "Expression";
  case NodeTypeFnCall:
    return "FnCall";
  }
  jane_panic("unreachable");
}

static void ast_print(AstNode *node, int indent) {
  for (int i = 0; i < indent; i += 1) {
    fprintf(stderr, " ");
  }
  switch (node->type) {
  case NodeTypeRoot:
    fprintf(stderr, "%s\n", node_type_str(node->type));
    for (int i = 0; i < node->data.root.fn_decls.length; i += 1) {
      AstNode *child = node->data.root.fn_decls.at(i);
      ast_print(child, indent + 2);
    }
    break;
  case NodeTypeFnDecl: {
    Buf *name_buf = &node->data.fn_decl.name;
    fprintf(stderr, "%s '%s'\n", node_type_str(node->type), buf_ptr(name_buf));
    for (int i = 0; i < node->data.fn_decl.params.length; i += 1) {
      AstNode *child = node->data.fn_decl.params.at(i);
      ast_print(child, indent + 2);
    }
    ast_print(node->data.fn_decl.return_type, indent + 2);
    ast_print(node->data.fn_decl.body, indent + 2);
    break;
  }
  default:
    fprintf(stderr, "%s\n", node_type_str(node->type));
    break;
  }
}

char cur_dir[1024];

int main(int argc, char **argv) {
  char *arg0 = argv[0];
  char *input_files = NULL;
  char *output_files = NULL;
  JaneList<char *> include_paths = {0};
  for (int i = 1; i < argc; i += 1) {
    char *arg = argv[i];
    if (arg[0] == '-' && arg[1] == '-') {
      if (strcmp(arg, "--version") == 0) {
        printf("jane v%s\n", JANE_VERSION_STRING);
        return EXIT_SUCCESS;
      } else if (i + 1 >= argc) {
        return usage(arg0);
      } else {
        i += 1;
        if (strcmp(arg, "--output") == 0) {
          output_files = argv[i];
        } else {
          return usage(arg0);
        }
      }
    } else if (arg[0] == '-' && arg[1] == 'I') {
      include_paths.append(arg + 2);
    } else if (!input_files) {
      input_files = arg;
    } else {
      return usage(arg0);
    }
  }
  if (!input_files || !output_files) {
    return usage(arg0);
  }
  FILE *in_f;
  Buf *cur_dir_path;
  if (strcmp(input_files, "-") == 0) {
    in_f = stdin;
    char *result = getcwd(cur_dir, sizeof(cur_dir));
    if (!result) {
      jane_panic("unable to get current working directory %s", strerror(errno));
    }
    cur_dir_path = buf_create_from_str(result);
  }
  Buf *in_data = fetch_file(in_f);
  fprintf(stderr, "original source code\n");
  fprintf(stderr, "----\n");
  fprintf(stderr, "%s\n", buf_ptr(in_data));
  JaneList<Token> *tokens = tokenize(in_data, cur_dir_path);
  fprintf(stderr, "\ntokens:\n");
  fprintf(stderr, "----\n");
  print_tokens(in_data, tokens);
  AstNode *root = ast_parse(in_data, tokens);
  assert(root);
  ast_print(root, 0);
  return EXIT_SUCCESS;
}