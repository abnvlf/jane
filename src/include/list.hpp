#ifndef JANE_LIST
#define JANE_LIST

#include "util.hpp"
#include <assert.h>

/**
 * @brief template class representing a dynamic list of items
 * @tparam T type of tiems stored in the list
 */
template <typename T> struct JaneList {
  T *items;     // pointer to the array of items
  int length;   // number of item current in the list
  int capacity; // capacity of the list (alocated size of the items array)

  // destructor to deinitialize list and deallocate memory
  void deinit() { free(items); }

  /**
   * @brief append an item to the end of the list
   * @param item the item to be append
   */
  void append(T item) {
    ensure_capacity(length + 1);
    items[length++] = item;
  }
  /**
   * @brief return const reference to the item at the specified index
   * @param index the index of the item to retrieve
   * @return const reference to the item at the specified index
   */
  const T &at(int index) const {
    assert(index >= 0);
    assert(index < length);
    return items[index];
  }
  /**
   * @brief retuern reference to the item at the specified index
   * @param index the index of the item to retrieve
   * @return reference to the item ath the specified index
   */
  T &at(int index) {
    assert(index >= 0);
    assert(index < length);
    return items[index];
  }
  /**
   * @brief remove and return the last item in the lit
   * @return the last item in the list
   */
  T pop() {
    assert(length >= 1);
    return items[--length];
  }
  /**
   * @brief add one item to the list and return it
   * @return the added item
   */
  void add_one() { return resize(length + 1); }

  const T &last() const {
    assert(length >= 1);
    return items[length - 1];
  }
  /**
   * @brief return reference to the last item in the list
   * @return reference to the last item in the last
   */
  T &last() {
    assert(length >= 1);
    return items[length - 1];
  }
  /**
   * @brief resize the list to the specified new length
   * @param new_length new length of the list
   */
  void resize(int new_length) {
    assert(new_length >= 0);
    ensure_capacity(new_length);
    length = new_length;
  }
  // clear the list
  void clear() { length = 0; }
  /**
   * @brief ensure that the list has the capacity to hold at least the
   specified
   * number of items
   * @param new_capacity the deisred capacity of the list
   */
  void ensure_capacity(int new_capacity) {
    int better_capacity = max(capacity, 16);
    while (better_capacity < new_capacity)
      better_capacity = better_capacity * 2;
    if (better_capacity != capacity) {
      items = reallocate_nonzero(items, better_capacity);
      capacity = better_capacity;
    }
  }
};

#endif // JANE_LIST