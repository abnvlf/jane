#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "include/util.hpp"

void jane_panic(const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  vfprintf(stderr, format, ap);
  fprintf(stderr, "\n");
  va_end(ap);
  abort();
}