#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "include/util.h"

void jane_panic(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  abort();
}

char *jane_alloc_snprintf(int *len, const char *format, ...) {
  va_list ap, ap2;
  va_start(ap, format);
  va_copy(ap2, ap);

  int len1 = vsnprintf(nullptr, 0, format, ap);
  assert(len1 >= 0);
  size_t required_size = len1 + 1;
  char *mem = allocate<char>(required_size);
  if (!mem) {
    return nullptr;
  }
  int len2 = vsnprintf(mem, required_size, format, ap2);
  assert(len2 == len1);
  va_end(ap2);
  va_end(ap);
  if (len) {
    *len = len1;
  }
  return mem;
}