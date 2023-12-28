#ifndef JANE_BUFFER
#define JANE_BUFFER

#include "list.hpp"
#include <assert.h>
#include <stdint.h>

// dyanimc buffer data structu with string formatting capability
struct Buf {
  JaneList<char> list;
};

/**
 * @brief format string and return dynamicallu allocated buffer
 * @param format formatted string
 * *@return dynamically allocated byffer with the formatted string
 */
Buf *buf_sprintf(const char *format, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief return the length of the buffer
 * @param buf the buffer
 * @return length of the buffer
 */
static inline int buf_len(Buf *buf) { return buf->list.length - 1; }

/**
 * @brief pointer to the underlying character array of the buffer
 * @param buf the buffer
 * @return pointer to the character array
 */
static inline char *buf_ptr(Buf *buf) { return buf->list.items; }

/**
 * @brief aresize the buffer to the specified new length
 * @param the buffer
 * @param new_len new length of the buffer
 */
static inline void buf_resize(Buf *buf, int new_len) {
  buf->list.resize(new_len + 1);
  buf->list.at(buf_len(buf)) = 0;
}

/**
 * @brief allocate and initialize a new buffer with default size
 * @return a  newly allocated buffer
 */
static inline Buf *buf_alloc(void) {
  Buf *buf = allocate<Buf>(1);
  buf_resize(buf, 0);
  return buf;
}

static inline Buf *buf_alloc_fixed(int size) {
  Buf *buf = allocate<Buf>(1);
  buf_resize(buf, size);
  return buf;
}

/**
 * @brief iniitialize buffer from a memory region
 * @param buf buffer to initialize
 * @param ptr pointer to the memory region
 * @param len length of memory region
 */
static inline void buf_init_from_mem(Buf *buf, char *ptr, int len) {
  buf->list.resize(len + 1);
  memcpy(buf_ptr(buf), ptr, len);
  buf->list.at(buf_len(buf)) = 0;
}

/**
 * @brief create new buffer from memory region
 * @param ptr pointer to the memory region
 * @param len length of the memory region
 * @return newly allocated buffer initialize with the specified memory region
 */
static inline Buf *buf_create_from_mem(char *ptr, int len) {
  Buf *buf = allocate<Buf>(1);
  buf_init_from_mem(buf, ptr, len);
  return buf;
}

/**
 * @brief crate a new buffer from a null-terminated string
 * @param str the null terminated string
 * @param newly allocated buffer initialize with the specified string
 */
static inline Buf *buf_create_from_str(char *str) {
  return buf_create_from_mem(str, strlen(str));
}

/**
 * @brief create a new buffer by slicing an existing buffer
 * @param in_buf input buffer
 * @param start the starting index of the lice
 * @param end the ending index of the slice
 * @return a newly allocated buffer representing the slice portion of the input
 *        buffer
 */
static inline Buf *buf_slice(Buf *in_buf, int start, int end) {
  assert(start >= 0);
  assert(end >= 0);
  assert(start < buf_len(in_buf));
  assert(end <= buf_len(in_buf));
  Buf *out_buf = allocate<Buf>(1);
  out_buf->list.resize(end - start + 1);
  memcpy(buf_ptr(out_buf), buf_ptr(in_buf) + start, end - start);
  out_buf->list.at(buf_len(out_buf)) = 0;
  return out_buf;
}

/**
 * @brief append memory region to the end of the buffer
 * @param buf the buffer
 * @param mem pointer ot the memory region
 * @param mem_len length of the memory region
 */
static inline void buf_append_mem(Buf *buf, const char *mem, int mem_len) {
  assert(mem_len >= 0);
  int old_len = buf_len(buf);
  buf_resize(buf, old_len + mem_len);
  memcpy(buf_ptr(buf) + old_len, mem, mem_len);
  buf->list.at(buf_len(buf)) = 0;
}

/**
 * @brief append a null-terminated string to the end of the buffer
 * @param buf the buffer
 * @param str null-terminated string
 */
static inline void buf_append_str(Buf *buf, const char *str) {
  buf_append_mem(buf, str, strlen(str));
}

/**
 * @brief append the cotnent of another buffer to the end of the buffer
 * @param buf the buffer
 * @param append_buf buffer to append
 */
static inline void buf_append_buf(Buf *buf, Buf *append_buf) {
  buf_append_mem(buf, buf_ptr(append_buf), buf_len(append_buf));
}

/**
 * @brief append single character to the end of the buffer
 * @param buf the buffer
 * @param c character to append
 */
static inline void buf_append_char(Buf *buf, uint8_t c) {
  buf_append_mem(buf, (const char *)&c, 1);
}

/**
 * @brief format a string and appends it to the end of the buffer
 * @param buf the buffer
 * @param format format string
 * @param ... additional format arguments
 */
void buf_appendf(Buf *buf, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * @brief check if the content of the buffer is equal to a memory region
 * @param buf buffer
 * @param mem pointer to the memory region
 * @param mem_len length of the memory region
 * @return `true` if the contet is equal, otherwise `false`
 */
static inline bool buf_eql_mem(Buf *buf, const char *mem, int mem_len) {
  if (buf_len(buf) != mem_len) {
    return false;
  }
  return memcmp(buf_ptr(buf), mem, mem_len) == 0;
}

static inline Buf *buf_dirname(Buf *buf) {
  if (buf_len(buf) <= 2) {
    jane_panic("TODO: buf_dirname");
  }
  int last_index = buf_len(buf) - 1;
  if (buf_ptr(buf)[buf_len(buf) - 1] == '/') {
    last_index = buf_len(buf) - 2;
  }
  for (int i = last_index; i >= 0; i -= 1) {
    uint8_t c = buf_ptr(buf)[i];
    if (c == '/') {
      return buf_slice(buf, 0, i);
    }
  }
  jane_panic("TODO: buf_dirname no slash");
}

#endif // JANE_BUFFER