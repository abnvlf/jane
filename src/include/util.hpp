#ifndef JANE_UTIL
#define JANE_UTIL

#include <assert.h>
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

void jane_panic(const char *format, ...) __attribute__((cold))
__attribute__((noreturn)) __attribute__((format(printf, 1, 2)));

/**
 * @brief allocate block memory for an array of element of type T without zero
 *          initialization
 * @tparam T the type of the elements
 * @param count the number of elements to allocate
 * @return pointer to the allcoated memory block
 */
template <typename T>
__attribute__((malloc)) static inline T *allocate_nonzero(size_t count) {
  T *ptr = reinterpret_cast<T *>(calloc(count, sizeof(T)));
  if (!ptr) {
    jane_panic("allocation failed");
  }
  return ptr;
}

/**
 * @brief allocate block of memory for an array of elements of type T with zero
 *          initialization
 * @tparam T the type of the element
 * @param count the number of elements to allocate
 * @return pointer to allocated memory block
 */
template <typename T>
__attribute__((malloc)) static inline T *allocate(size_t count) {
  T *ptr = reinterpret_cast<T *>(calloc(count, sizeof(T)));
  if (!ptr) {
    jane_panic("allocation failed");
  }
  return ptr;
}

/**
 * @brief reallocate block of memory for an arary of element of type T without
 *          zero initialization
 * @tparam T type of the elements
 * @param old pointer to the previously allocated memory block
 * @param new_count new number of element to allocate
 * @return pointer to the reallocated memory block
 */
template <typename T>
static inline T *reallocate_nonzero(T *old, size_t new_count) {
  T *ptr = reinterpret_cast<T *>(realloc(old, new_count * sizeof(T)));
  if (!ptr) {
    jane_panic("allocation failed");
  }
  return ptr;
}

/**
 * @brief allocate and format a string using sprintf syntax
 * @param len pointer to integer that will store the length of the allocated
 *             string
 * @param format format string
 * @param ... additional argument for format
 * @return dynamically allocated string with the formatted content
 */
char *jane_alloc_sprintf(int *len, const char *format, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * @brief return the length of statically sized array
 * @tparam T the type of the array elements
 * @tparam n the size of the array
 * @param array input array
 * @return length of the array
 */
template <typename T, long n> constexpr long array_length(const T (&)[n]) {
  return n;
}

/**
 * @brief return maximym of two value
 * @tparam T type of the value
 * @param a first value
 * @param b second value
 * @return maximum of the two values
 */
template <typename T> static inline T max(T a, T b) { return (a >= b) ? a : b; }

/**
 * @brief return minimum of two values
 * @tparam T type of the value
 * @param first the first value
 * @param second the second value
 * @return minimum of the two values
 */
template <typename T> static inline T min(T a, T b) { return (a <= b) ? a : b; }

/**
 * @brief clamps value between minimum and maximum value
 * @tparam T type of the value
 * @param min_value minimum allowed value
 * @param value value to clamp
 * @param max_value maximum allowed value
 * @return clamped valud
 */
template <typename T> static inline T clamp(T min_value, T value, T max_value) {
  return max(min(value, max_value), min_value);
}

#endif // JANE_UTIL