#pragma once
#include "chiba_utils_abi.h"
#include "chiba_utils_basic_types.h"
#include "chiba_utils_memory.h"
#include "chiba_utils_visibility.h"

// 只插入的哈希表 - 使用开放寻址法（线性探测）
// 键类型：str，值类型：u64

typedef enum {
  VHASHTABLE_EMPTY = 0,
  VHASHTABLE_OCCUPIED = 1
} VHashTableEntryState;

typedef struct {
  u64 hash;
  str key;
  API_FUNC_PTR value;
  VHashTableEntryState state;
} VHashTableEntry;

typedef struct {
  VHashTableEntry *entries;
  u64 capacity;
  u64 count;
  u64 load_factor_percent;
} VHashTable;

UTILS u64 vhashtable_hash_str(str s) { return _internal_str_hash(s); }

UTILS str str_copy(str s) {
  i8 *new_data = (i8 *)malloc(s.len);
  if (new_data == NULL) {
    return (str){.data = NULL, .len = 0};
  }
  memcpy(new_data, s.data, s.len);
  return (str){.data = new_data, .len = s.len};
}

UTILS VHashTable *vhashtable_create() {
  u64 capacity = 128;
  VHashTable *table = (VHashTable *)malloc(sizeof(VHashTable));
  if (table == NULL)
    return NULL;

  table->entries =
      (VHashTableEntry *)malloc(sizeof(VHashTableEntry) * capacity);
  if (table->entries == NULL) {
    free(table);
    return NULL;
  }

  for (u64 i = 0; i < capacity; i++) {
    table->entries[i].state = VHASHTABLE_EMPTY;
    table->entries[i].hash = 0;
    table->entries[i].key = (str){.data = NULL, .len = 0};
    table->entries[i].value = 0;
  }

  table->capacity = capacity;
  table->count = 0;
  table->load_factor_percent = 75;
  return table;
}

UTILS void vhashtable_resize(VHashTable *table) {
  if (table == NULL) {
    fprintf(
        stderr,
        "%s%s%s:%d FATAL ERROR: vhashtable_resize called with NULL table%s\n",
        ANSI_BOLD, ANSI_RED, __FILE__, __LINE__, ANSI_RESET);
    abort();
  }

  u64 new_capacity = table->capacity << 1;
  VHashTableEntry *new_entries =
      (VHashTableEntry *)malloc(sizeof(VHashTableEntry) * new_capacity);
  if (new_entries == NULL) {
    fprintf(stderr,
            "%s%s%s:%d FATAL ERROR: vhashtable_resize failed to allocate "
            "memory%s\n",
            ANSI_BOLD, ANSI_RED, __FILE__, __LINE__, ANSI_RESET);
    abort();
  }

  for (u64 i = 0; i < new_capacity; i++) {
    new_entries[i].state = VHASHTABLE_EMPTY;
    new_entries[i].hash = 0;
    new_entries[i].key = (str){.data = NULL, .len = 0};

    new_entries[i].value = 0;
  }

  VHashTableEntry *old_entries = table->entries;
  u64 old_capacity = table->capacity;
  table->entries = new_entries;
  table->capacity = new_capacity;
  table->count = 0;

  for (u64 i = 0; i < old_capacity; i++) {
    if (old_entries[i].state == VHASHTABLE_OCCUPIED) {
      u64 index = old_entries[i].hash & (new_capacity - 1);
      while (new_entries[index].state == VHASHTABLE_OCCUPIED) {
        index = (index + 1) & (new_capacity - 1);
      }
      new_entries[index].hash = old_entries[i].hash;
      new_entries[index].key = old_entries[i].key;
      new_entries[index].value = old_entries[i].value;
      new_entries[index].state = VHASHTABLE_OCCUPIED;
      table->count++;
    }
  }

  free(old_entries);
}

UTILS void vhashtable_insert(VHashTable *table, str key, API_FUNC_PTR value) {
  if (table == NULL || key.data == NULL) {
    fprintf(
        stderr,
        "%s%s%s:%d FATAL ERROR: vhashtable_insert called with NULL table or "
        "key%s\n",
        ANSI_BOLD, ANSI_RED, __FILE__, __LINE__, ANSI_RESET);
    abort();
  }

  u64 threshold = (table->capacity * table->load_factor_percent) / 100;
  if (table->count >= threshold) {
    vhashtable_resize(table);
  }

  u64 hash = vhashtable_hash_str(key);
  u64 index = hash & (table->capacity - 1);
  u64 start_index = index;

  while (table->entries[index].state == VHASHTABLE_OCCUPIED) {
    if (table->entries[index].hash == hash &&
        str_equals(table->entries[index].key, key)) {
      table->entries[index].value = value;
      return;
    }
    index = (index + 1) & (table->capacity - 1);
    if (index == start_index) {
      fprintf(stderr,
              "%s%s%s:%d FATAL ERROR: vhashtable_insert could not find empty "
              "slot%s\n",
              ANSI_BOLD, ANSI_RED, __FILE__, __LINE__, ANSI_RESET);
      abort();
    }
  }

  table->entries[index].hash = hash;
  table->entries[index].key = str_copy(key);
  table->entries[index].value = value;
  table->entries[index].state = VHASHTABLE_OCCUPIED;
  if (table->entries[index].key.data == NULL) {
    fprintf(stderr,
            "%s%s%s:%d FATAL ERROR: vhashtable_insert failed to copy key%s\n",
            ANSI_BOLD, ANSI_RED, __FILE__, __LINE__, ANSI_RESET);
    abort();
  }

  table->count++;
}

UTILS API_FUNC_PTR vhashtable_get(VHashTable *table, str key) {
  if (table == NULL || key.data == NULL)
    return 0;

  u64 hash = vhashtable_hash_str(key);
  u64 index = hash & (table->capacity - 1);
  u64 start_index = index;

  while (table->entries[index].state == VHASHTABLE_OCCUPIED) {
    if (table->entries[index].hash == hash &&
        str_equals(table->entries[index].key, key)) {
      return table->entries[index].value;
    }
    index = (index + 1) & (table->capacity - 1);
    if (index == start_index)
      break;
  }
  return 0;
}

UTILS bool vhashtable_contains(VHashTable *table, str key) {
  if (table == NULL || key.data == NULL)
    return false;

  u64 hash = vhashtable_hash_str(key);
  u64 index = hash & (table->capacity - 1);
  u64 start_index = index;

  while (table->entries[index].state == VHASHTABLE_OCCUPIED) {
    if (table->entries[index].hash == hash &&
        str_equals(table->entries[index].key, key)) {
      return true;
    }
    index = (index + 1) & (table->capacity - 1);
    if (index == start_index)
      break;
  }
  return false;
}

UTILS u64 vhashtable_size(VHashTable *table) {
  return table ? table->count : 0;
}

UTILS u64 vhashtable_capacity(VHashTable *table) {
  return table ? table->capacity : 0;
}

UTILS f64 vhashtable_load_factor(VHashTable *table) {
  if (table == NULL || table->capacity == 0)
    return 0.0;
  return (f64)table->count / (f64)table->capacity;
}

#define VHASHTABLE_INSERT_CSTR(table, cstr, value)                             \
  vhashtable_insert(table, STR(cstr), value)
#define VHASHTABLE_GET_CSTR(table, cstr) vhashtable_get(table, STR(cstr))
#define VHASHTABLE_CONTAINS_CSTR(table, cstr)                                  \
  vhashtable_contains(table, STR(cstr))
