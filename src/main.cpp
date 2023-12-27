#include "../config.h"
#include "include/list.h"
#include "include/token.h"
#include "include/util.h"

static int usage(char *arg0) {
  fprintf(stderr,
          "usage: %s --output outfile hello.jn\n, --version (print version)\n",
          arg0);
  return EXIT_FAILURE;
}

static struct Buf *fetch_file(FILE *f) {
  int fd = fileno(f);
  struct stat st;
  if (fstat(fd, &st)) {
    jane_panic("unable to stat file: %s", strerror(errno));
  }
  off_t big_size = st.st_size;
  if (big_size > INT_MAX) {
    jane_panic("file to big awkoakowa");
  }
  int size = (int)big_size;
  Buf *buf = alloc_buf(size);
  size_t amt_read = fread(buf->ptr, 1, buf->len, f);
  if (amt_read != (size_t)buf->len) {
    jane_panic("error reading: %s", strerror(errno));
  }
  return buf;
}

int main(int argc, char **argv) {
  char *arg0 = argv[0];
  char *in_files = NULL;
  char *output_file = NULL;
  for (int i = 1; i < argc; i += 1) {
    char *arg = argv[i];
    if (arg[0] == '-' && arg[1] == '-') {
      if (strcmp(arg, "--version") == 0) {
        printf("%s\n", JANE_VERSION_STRING);
        return EXIT_SUCCESS;
      } else if (i + 1 >= argc) {
        return usage(arg0);
      } else {
        i += 1;
        if (strcmp(arg, "--output") == 0) {
          output_file = argv[i];
        } else {
          return usage(arg0);
        }
      }
    } else if (!in_files) {
      in_files = arg;
    } else {
      return usage(arg0);
    }
  }

  if (!in_files || !output_file) {
    return usage(arg0);
  }

  FILE *in_f;
  if (strcmp(in_files, "-") == 0) {
    in_f = stdin;
  } else {
    in_f = fopen(in_files, "rb");
    if (!in_f) {
      jane_panic("unable to open `%s` for reading: `%s`\n", in_file,
                 strerror(errno));
    }
  }
  struct Buf *in_data = fetch_file(in_f);
  fprintf(stderr, "%s\n", in_data->ptr);
  JaneList<Token> *tokens = tokenize(in_data);
  print_token(in_data, tokens);
  return EXIT_SUCCESS;
}