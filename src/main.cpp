#include "../config.h"
#include "include/buffer.hpp"
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

static int build(const char *arg0, const char *in_files, const char *out_files,
                 JaneList<char *> *include_paths) {
  static char cur_dir[1024];
  if (!in_files || !out_files) {
    return usage(arg0);
  }
  FILE *in_f;
  Buf *cur_dir_path;
  if (strcmp(in_files, "-") == 0) {
    in_f = stdin;
    char *result = getcwd(cur_dir, sizeof(cur_dir));
    if (!result) {
      jane_panic("error: unable to get current working directory: %s",
                 strerror(errno));
    }
    cur_dir_path = buf_create_from_str(result);
  } else {
    in_f = fopen(in_files, "rb");
    if (!in_f) {
      jane_panic("error: unable to open %s for reading %s\n", in_files,
                 strerror(errno));
    }
    cur_dir_path = buf_dirname(buf_create_from_str(in_files));
  }
  Buf *in_data = fetch_file(in_f);

  fprintf(stderr, "original code\n");
  fprintf(stderr, "----\n");
  fprintf(stderr, "%s\n", buf_ptr(in_data));
  JaneList<Token> *tokens = tokenize(in_data, cur_dir_path);
  fprintf(stderr, "\ntokens:\n");
  fprintf(stderr, "----\n");
  print_tokens(in_data, tokens);
  AstNode *root = ast_parse(in_data, tokens);
  assert(root);
  ast_print(root, 0);
  return 0;
}

enum ElfType {
  ElfTypeRelocatable,
  ElfTypeExecutable,
  ElfTypeShared,
  ElfTypeCore,
};

enum ElfArch {
  ElfArchSparc,
  ElfArchx86,
  ElfArchMips,
  ElfArchPowerPc,
  ElfArchArm,
  ElfArchSuperH,
  ElfArchIA_64,
  ElfArchx86_64,
  ElfArchAArch64,
};

enum ElfSectionType {
  SHT_NULL = 0,
  SHT_PROGBITS = 1,
  SHT_SYMTAB = 2,
  SHT_STRTAB = 3,
  SHT_RELA = 4,
  SHT_HASH = 5,
  SHT_DYNAMIC = 6,
  SHT_NOTE = 7,
  SHT_NOBITS = 8,
  SHT_REL = 9,
  SHT_SHLIB = 10,
  SHT_DYNSYM = 11,
  SHT_INIT_ARRAY = 14,
  SHT_FINI_ARRAY = 15,
  SHT_PREINIT_ARRAY = 16,
  SHT_GROUP = 17,
  SHT_SYMTAB_SHNDX = 18,
  SHT_LOOS = 0x60000000,
  SHT_HIOS = 0x6fffffff,
  SHT_LOPROC = 0x70000000,
  SHT_HIPROC = 0x7fffffff,
  SHT_LOUSER = 0x80000000,
  SHT_HIUSER = 0xffffffff,
};

struct Elf32SectionHeader {
  uint32_t name;
  uint32_t type;
  uint32_t flags;
  size_t addr;
  off_t offset;
  uint32_t size;
  uint32_t link;
  uint32_t info;
  uint32_t addralign;
  uint32_t entsize;
};

struct Elf64SectionHeader {
  uint32_t name;
  uint32_t type;
  uint64_t flags;
  size_t addr;
  off_t offset;
  uint64_t size;
  uint32_t link;
  uint32_t info;
  uint64_t addralign;
  uint64_t entsize;
};

struct Elf {
  bool is_64;
  bool is_little_endian;
  ElfType type;
  ElfArch arch;
  uint64_t entry_addr;
  uint64_t program_header_offset;
  uint64_t section_header_offset;
  int sh_entry_count;
  int string_section_index;
  union {
    Elf32SectionHeader *elf32_section_headers;
    Elf64SectionHeader *elf64_section_headers;
  };
};

static int parse_elf(Elf *elf, Buf *buf) {
  int len = buf_len(buf);
  if (len < 52) {
    return ErrorInvalidFormat;
  }
  char *ptr = buf_ptr(buf);
  static const int magic_size = 4;
  static const char magic[magic_size] = {0x7f, 'E', 'L', 'F'};
  if (memcmp(ptr, magic, magic_size) != 0) {
    return ErrorInvalidFormat;
  }
  ptr += magic_size;
  if (*ptr == 1) {
    elf->is_64 = false;
  } else if (*ptr == 2) {
    elf->is_64 = true;
    if (len < 64)
      return ErrorInvalidFormat;
  } else {
    return ErrorInvalidFormat;
  }
  ptr += 1;

  if (*ptr == 1) {
    elf->is_little_endian = true;
  } else if (*ptr == 2) {
    elf->is_little_endian = false;
    jane_panic("error: can only handle little endian");
  } else {
    return ErrorInvalidFormat;
  }
  ptr += 1;
  if (*ptr != 1) {
    return ErrorInvalidFormat;
  }
  uint16_t *type_number = (uint16_t *)&buf_ptr(buf)[0x10];
  if (*type_number == 1) {
    elf->type = ElfTypeRelocatable;
  } else if (*type_number == 2) {
    elf->type = ElfTypeExecutable;
  } else if (*type_number == 3) {
    elf->type = ElfTypeShared;
  } else if (*type_number == 4) {
    elf->type = ElfTypeCore;
  } else {
    return ErrorInvalidFormat;
  }

  uint16_t *arch = (uint16_t *)&buf_ptr(buf)[0x12];
  if (*arch == 0x02) {
    elf->arch = ElfArchSparc;
  } else if (*arch == 0x03) {
    elf->arch = ElfArchx86;
  } else if (*arch == 0x08) {
    elf->arch = ElfArchMips;
  } else if (*arch == 0x14) {
    elf->arch = ElfArchPowerPc;
  } else if (*arch == 0x28) {
    elf->arch = ElfArchArm;
  } else if (*arch == 0x2A) {
    elf->arch = ElfArchSuperH;
  } else if (*arch == 0x32) {
    elf->arch = ElfArchIA_64;
  } else if (*arch == 0x3E) {
    elf->arch = ElfArchx86_64;
  } else if (*arch == 0xb7) {
    elf->arch = ElfArchAArch64;
  } else {
    return ErrorInvalidFormat;
  }

  uint32_t *elf_vers = (uint32_t *)&buf_ptr(buf)[0x14];
  if (*elf_vers != 1) {
    return ErrorInvalidFormat;
  }
  ptr = &buf_ptr(buf)[0x18];
  if (elf->is_64) {
    elf->entry_addr = *((uint64_t *)ptr);
    ptr += 8;
    elf->program_header_offset = *((uint64_t *)ptr);
    ptr += 8;
    elf->section_header_offset = *((uint64_t *)ptr);
    ptr += 8;
  } else {
    elf->entry_addr = *((uint32_t *)ptr);
    ptr += 4;
    elf->program_header_offset = *((uint32_t *)ptr);
    ptr += 4;
    elf->section_header_offset = *((uint32_t *)ptr);
    ptr += 4;
  }

  ptr += 4;
  uint16_t header_size = *((uint16_t *)ptr);
  if ((elf->is_64 && header_size != 64) || (!elf->is_64 && header_size != 52)) {
    return ErrorInvalidFormat;
  }
  ptr += 2;
  uint16_t ph_entry_size = *((uint16_t *)ptr);
  ptr += 2;
  uint16_t ph_entry_count = *((uint16_t *)ptr);
  ptr += 2;
  uint16_t sh_entry_size = *((uint16_t *)ptr);
  ptr += 2;
  uint16_t sh_entry_count = *((uint16_t *)ptr);
  ptr += 2;
  uint16_t sh_name_index = *((uint16_t *)ptr);
  ptr += 2;

  elf->string_section_index = sh_name_index;
  if (elf->string_section_index >= sh_entry_count) {
    return ErrorInvalidFormat;
  }

  long sh_byte_count = ((long)sh_entry_size) * ((long)sh_entry_count);
  long end_sh = ((long)elf->section_header_offset) + sh_byte_count;
  long end_ph = ((long)elf->program_header_offset) +
                ((long)ph_entry_size) * ((long)ph_entry_count);

  if (len < end_sh || len < end_ph) {
    return ErrorInvalidFormat;
  }
  ptr = &buf_ptr(buf)[elf->section_header_offset];
  if (elf->is_64) {
    if (sh_entry_size != sizeof(Elf64SectionHeader)) {
      return ErrorInvalidFormat;
    }
    elf->elf64_section_headers = allocate<Elf64SectionHeader>(sh_entry_count);
    memcpy(elf->elf64_section_headers, ptr, sh_byte_count);
    elf->sh_entry_count = sh_entry_count;
    Elf64SectionHeader *string_section =
        &elf->elf64_section_headers[elf->string_section_index];
    if (string_section->type != SHT_STRTAB) {
      return ErrorInvalidFormat;
    }
    for (int i = 0; i < elf->sh_entry_count; i += 1) {
      Elf64SectionHeader *section = &elf->elf64_section_headers[i];
      if (section->type != SHT_NOBITS) {
        long file_end_offset = ((long)section->offset) + ((long)section->size);
        if (len < file_end_offset) {
          return ErrorInvalidFormat;
        }
      }
    }
    for (int i = 0; i < elf->sh_entry_count; i += 1) {
      Elf64SectionHeader *section = &elf->elf64_section_headers[i];
      if (section->type == SHT_NULL) {
        continue;
      }
      long name_offset = section->name;
      ptr = &buf_ptr(buf)[string_section->offset + name_offset];
      fprintf(stderr, "section: %s\n", ptr);
      fprintf(stderr, "  start: 0x%" PRIx64 "\n", (uint64_t)section->offset);
      fprintf(stderr, "    end: 0x%" PRIx64 "\n",
              (uint64_t)section->offset + (uint64_t)section->size);

      if (section->type == SHT_SYMTAB) {
        fprintf(stderr, "  symtab\n");
      } else if (section->type == SHT_DYNSYM) {
        fprintf(stderr, "  dynsym\n");
        jane_panic("TODO SHT_DYNSYM");
      }
    }
  } else {
    if (sh_entry_size != sizeof(Elf32SectionHeader)) {
      return ErrorInvalidFormat;
    }
    jane_panic("TODO: working on 32-bit elf");
  }
  return 0;
}

static int link(const char *arg0, const char *in_files, const char *out_files) {
  if (!in_files || !out_files) {
    return usage(arg0);
  }
  FILE *in_f;
  if (strcmp(in_files, "-") == 0) {
    in_f = stdin;
  } else {
    in_f = fopen(in_files, "rb");
    if (!in_f) {
      jane_panic("unable to open %s for reading: %s\n", in_files,
                 strerror(errno));
    }
  }
  Buf *in_data = fetch_file(in_f);
  Elf elf = {0};
  int err;
  if ((err = parse_elf(&elf, in_data))) {
    fprintf(stderr, "unable to parse ELF: %s\n", err_str(err));
    return 1;
  }
  fprintf(stderr, "ELF is 64? %d\n", (int)elf.is_64);
  fprintf(stderr, "arch: %d\n", (int)elf.arch);
  fprintf(stderr, "exe? %d\n", (int)(elf.type == ElfTypeExecutable));
  fprintf(stderr, "entry: 0x%" PRIx64 "\n", elf.entry_addr);
  return 0;
}

char cur_dir[1024];

enum Cmd {
  CmdNone,
  CmdBuild,
  CmdLink,
};

int main(int argc, char **argv) {
  char *arg0 = argv[0];
  char *input_files = NULL;
  char *output_files = NULL;
  JaneList<char *> include_paths = {0};
  Cmd cmd = CmdNone;

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
    } else if (cmd == CmdNone) {
      if (strcmp(arg, "build") == 0) {
        cmd = CmdBuild;
      } else if (strcmp(arg, "link") == 0) {
        cmd = CmdLink;
      } else {
        fprintf(stderr, "error: error command %s\n", arg);
        return usage(arg0);
      }
    } else {
      switch (cmd) {
      case CmdNone:
        jane_panic("error: unreachable");
      case CmdBuild:
        if (!input_files) {
          input_files = arg;
        } else {
          return usage(arg0);
        }
        break;
      case CmdLink:
        if (!input_files) {
          input_files = arg;
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
    return build(arg0, input_files, output_files, &include_paths);
    break;
  case CmdLink:
    return link(arg0, input_files, output_files);
    break;
  }
}