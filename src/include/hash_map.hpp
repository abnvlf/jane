#ifndef JANE_HASH_MAP
#define JANE_HASH_MAP

#include "util.hpp"
#include <stdint.h>

template <typename K, typename V, uint32_t (*HashFunction)(K key),
          bool (*EqualFn)(K a, K b)>
class HashMap {

public:
  struct Entry {
    bool used;
    int distance_from_start_index;
    K key;
    V value;
  };

  void init(int capacity) { init_capacity(capacity); }
  void deinit(void) { free(_entries); }
  void clear() {
    for (int i = 0; i < _capacity; i += 1) {
      _entries[i].used = false;
    }
    _size = 0;
    _max_distance_from_start_index = 0;
    _modification_count += 1;
  }
  int size() const { return _size; }
  void put(const K &key, const V &value) {
    _modification_count += 1;
    internal_put(key, value);

    if (_size * 5 >= _capacity * 4) {
      Entry *old_entries = _entries;
      int old_capacity = _capacity;
      init_capacity(_capacity * 2);
      for (int i = 0; i < old_capacity; i += 1) {
        Entry *old_entry = &old_entries[i];
        if (old_entry->used) {
          internal_put(old_entry->key, old_entry->value);
        }
      }
      free(old_entries);
    }
  }

  const V &get(const K &key) const {
    Entry *entry = internal_get(key);
    if (!entry) {
      jane_panic("key not found");
    }
    return entry->value;
  }

  Entry *maybe_get(const K &key) const { return internal_get(key); }

  void remove(const K &key) {
    _modification_count += 1;
    int start_index = key_to_index(key);
    for (int roll_over = 0; roll_over <= _max_distance_from_start_index;
         roll_over += 1) {
      int index = (start_index + roll_over) % _capacity;
      Entry *entry = &_entries[index];
      if (!entry->used) {
        jane_panic("key not found");
      }
      if (!EqualFn(entry->key, key)) {
        continue;
      }
      for (; roll_over < _capacity; roll_over += 1) {
        int next_index = (start_index + roll_over + 1) % _capacity;
        Entry *next_entry = &_entries[next_index];
        if (!next_entry->used || next_entry->distance_from_start_index == 0) {
          entry->used = false;
          _size -= 1;
          return;
        }
        *entry = *next_entry;
        entry->distance_from_start_index -= 1;
        entry = next_entry;
      }
      jane_panic("shifting everything in the table");
    }
    jane_panic("key not found");
  }

  class Iterator {
  public:
    Entry *next() {
      if (_initial_modification_count != _table->_modification_count) {
        jane_panic("concurrent modification");
      }
      if (_count >= _table->size()) {
        return NULL;
      }
      for (; _index < _table->_capacity; _index += 1) {
        Entry *entry = &_table->_entries[_index];
        if (entry->used) {
          _index += 1;
          _count += 1;
          return entry;
        }
      }
      jane_panic("no next item");
    }

  private:
    const HashMap *_table;
    int _count = 0;
    int _index = 0;
    uint32_t _initial_modification_count;
    Iterator(const HashMap *table)
        : _table(table),
          _initial_modification_count(table->_modification_count) {}
    friend HashMap;
  };

  Iterator entry_iterator() const { return Iterator(this); }

private:
  Entry *_entries;
  int _capacity;
  int _size;
  int _max_distance_from_start_index;
  uint32_t _modification_count = 0;

  void init_capacity(int capacity) {
    _capacity = capacity;
    _entries = allocate<Entry>(_capacity);
    _size = 0;
    _max_distance_from_start_index = 0;
    for (int i = 0; i < _capacity; i += 1) {
      _entries[i].used = false;
    }
  }
  void internal_put(K key, V value) {
    int start_index = key_to_index(key);
    for (int roll_over = 0, distance_from_start_index = 0;
         roll_over < _capacity;
         roll_over += 1, _max_distance_from_start_index += 1) {
      int index = (start_index + roll_over) % _capacity;
      Entry *entry = &_entries[index];
      if (entry->used && !EqualFn(entry->key, key)) {
        if (entry->distance_from_start_index < distance_from_start_index) {
          Entry tmp = *entry;
          if (entry->distance_from_start_index < distance_from_start_index) {
            _max_distance_from_start_index = distance_from_start_index;
          }
          *entry = {
              true,
              distance_from_start_index,
              key,
              value,
          };
          key = tmp.key;
          value = tmp.value;
          distance_from_start_index = tmp.distance_from_start_index;
        }
        continue;
      }
      if (!entry->used) {
        _size += 1;
      }
      if (distance_from_start_index > _max_distance_from_start_index) {
        _max_distance_from_start_index = distance_from_start_index;
      }
      *entry = {
          true,
          distance_from_start_index,
          key,
          value,
      };
      return;
    }
    jane_panic("atmint: put into hashmap");
  }
  Entry *internal_get(const K &key) const {
    int start_index = key_to_index(key);
    for (int roll_over = 0; roll_over <= _max_distance_from_start_index;
         roll_over += 1) {
      int index = (start_index + roll_over) % _capacity;
      Entry *entry = &_entries[index];
      if (!entry->used) {
        return NULL;
      }
      if (EqualFn(entry->key, key)) {
        return entry;
      }
    }
    return NULL;
  }
  int key_to_index(const K &key) const {
    return (int)(HashFunction(key) % ((uint32_t)_capacity));
  }
};

#endif // JANE_HASH_MAP