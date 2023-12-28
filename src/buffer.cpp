#include "include/buffer.hpp"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

Buf *buf_sprintf(const char *format, ...) {
  va_list ap, ap2;
  va_start(ap, format);
  va_copy(ap2, ap);
  // determine the lenght of the formatted string
  int len1 = vsnprintf(nullptr, 0, format, ap);
  assert(len1 >= 0);
  // calculate the required size for the buffer
  size_t required_size = len1 + 1;
  // allocate fixed-size  buffer
  Buf *buf = buf_alloc_fixed(len1);
  // format the string into the buffer
  int len2 = vsnprintf(buf_ptr(buf), required_size, format, ap2);
  assert(len2 == len1);
  // cleanup the argument list
  va_end(ap2);
  va_end(ap);
  return buf;
}

// append a formatted string to an existing buffer
void buf_appendf(Buf *buf, const char *format, ...) {
  // initialize two raguments list
  va_list ap, ap2;
  va_start(ap, format);
  va_copy(ap2, ap);
  // determine the length of the formatted string
  int len1 = vsnprintf(nullptr, 0, format, ap);
  assert(len1 >= 0);
  // calculate the required size for the buffer
  size_t required_size = len1 + 1;
  // get the original length of the buffer
  int orig_len = buf_len(buf);
  // resize the buffer to accomodate the new string
  buf_resize(buf, orig_len + required_size);
  // format the string into the buffer at the appropriate position
  int len2 = vsnprintf(buf_ptr(buf) + orig_len, required_size, format, ap2);
  assert(len2 == len1);
  // cleanup the argument list
  va_end(ap2);
  va_end(ap);
}