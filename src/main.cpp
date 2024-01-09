#include "../config.h"
#include "include/buffer.hpp"
#include "include/codegen.hpp"
#include "include/os.hpp"

#include <stdio.h>

static int usage(const char *arg0) {
  fprintf(stderr,
          "usage: %s (command) (option) target\n"
          "command\n"
          "build    [create an executable from target files]\n"
          "link     [turn `.o` file to executable files]\n"
          "option\n"
          "--output (file)   [output file]\n"
          "--version  [display version of jane language]\n"
          "--release  [build with optimization on]\n"
          "--strip    [exclude debug symbol]\n"
          "--static   [build a static executable]\n"
          "-Ipath     [add path to haeder include path]\n"
          "--export (exe | lib | obj) override output type\n",
          arg0);
  return EXIT_FAILURE;
}

static int version(void) {
  printf("first. an organism that convert caffeine into code, usually late at "
         "night\n"
         "second. a person who solves a problem you didn't know you had in a "
         "way don't understand\n");
  printf("jane v%s\n", JANE_VERSION_STRING);
  return EXIT_SUCCESS;
}

struct Build {
  const char *input_file;
  const char *output_file;
  bool release;
  bool strip;
  bool is_static;
  OutType out_type;
  const char *output_name;
  bool verbose;
};

static int build(const char *arg0, Build *b) {
  if (!b->input_file) {
    return usage(arg0);
  }
  Buf in_file_buf = BUF_INIT;
  buf_init_from_str(&in_file_buf, b->input_file);

  Buf root_source_dir = BUF_INIT;
  Buf root_source_code = BUF_INIT;
  Buf root_source_name = BUF_INIT;
  if (buf_eql_str(&in_file_buf, "-")) {
    os_get_cwd(&root_source_dir);
    os_fetch_file(stdin, &root_source_code);
    buf_init_from_str(&root_source_name, "");
  } else {
    os_path_split(&in_file_buf, &root_source_dir, &root_source_name);
    os_fetch_file_path(buf_create_from_str(b->input_file), &root_source_code);
  }

  CodeGen *g = codegen_create(&root_source_dir);
  codegen_set_build_type(g, b->release ? CodeGenBuildTypeRelease
                                       : CodeGenBuildTypeDebug);
  codegen_set_strip(g, b->strip);
  codegen_set_is_static(g, b->is_static);
  if (b->out_type != OutTypeUnknown) {
    codegen_set_out_type(g, b->out_type);
  }
  if (b->out_type) {
    codegen_set_out_name(g, buf_create_from_str(b->output_name));
  }
  codegen_set_verbose(g, buf_create_from_str(b->output_name));
  codegen_add_root_code(g, &root_source_code, &root_source_code);
  codegen_link(g, b->output_file);
  return 0;
}

enum Cmd {
  CmdNone,
  CmdBuild,
  CmdVersion,
};

int main(int argc, char **argv) {
  char *arg0 = argv[0];

  Build b = {0};
  Cmd cmd = CmdNone;
  for (int i = 1; i < argc; i += 1) {
    char *arg = argv[i];
    if (arg[0] == '-' && arg[1] == '-') {
      if (strcmp(arg, "--release") == 0) {
        b.release = true;
      } else if (strcmp(arg, "--strip") == 0) {
        b.strip = true;
      } else if (strcmp(arg, "--static") == 0) {
        b.is_static = true;
      } else if (i + 1 >= argc) {
        return usage(arg0);
      } else {
        i += 1;
        if (strcmp(arg, "--output") == 0) {
          b.output_file = argv[i];
        } else if (strcmp(arg, "--export") == 0) {
          if (strcmp(argv[i], "exe") == 0) {
            b.out_type = OutTypeExe;
          } else if (strcmp(argv[i], "lib") == 0) {
            b.out_type = OutTypeLib;
          } else if (strcmp(argv[i], "obj") == 0) {
            b.out_type = OutTypeObj;
          } else {
            return usage(arg0);
          }
        } else if (strcmp(arg, "--name") == 0) {
          b.output_name = argv[i];
        } else {
          return usage(arg0);
        }
      }
    } else if (cmd == CmdNone) {
      if (strcmp(arg, "build") == 0) {
        cmd = CmdBuild;
      } else if (strcmp(arg, "version") == 0) {
        cmd = CmdVersion;
      } else {
        fprintf(stderr, "unrecognized command: %s\n", arg);
        return usage(arg0);
      }
    } else {
      switch (cmd) {
      case CmdNone:
        jane_unreachable();
      case CmdBuild:
        if (!b.input_file) {
          b.input_file = arg;
        } else {
          return usage(arg0);
        }
        break;
      case CmdVersion:
        return usage(arg0);
      }
    }
  }

  switch (cmd) {
  case CmdNone:
    return usage(arg0);
  case CmdBuild:
    return build(arg0, &b);
  case CmdVersion:
    return version();
  }
  jane_unreachable();
}
