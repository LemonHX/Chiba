#pragma once

#include "chiba_utils_basic_types.h"
#include "chiba_utils_visibility.h"

#define get_type_from_buffer(type)                                             \
  UTILS type get_##type(const u8 *tab) {                                       \
    type v;                                                                    \
    memcpy(&v, tab, sizeof(v));                                                \
    return v;                                                                  \
  }

#define put_type_to_buffer(type)                                               \
  UTILS void put_##type(u8 *tab, type val) { memcpy(tab, &val, sizeof(val)); }

get_type_from_buffer(u8);
get_type_from_buffer(u16);
get_type_from_buffer(u32);
get_type_from_buffer(u64);
get_type_from_buffer(i8);
get_type_from_buffer(i16);
get_type_from_buffer(i32);
get_type_from_buffer(i64);
get_type_from_buffer(f32);
get_type_from_buffer(f64);

put_type_to_buffer(u8);
put_type_to_buffer(u16);
put_type_to_buffer(u32);
put_type_to_buffer(u64);
put_type_to_buffer(i8);
put_type_to_buffer(i16);
put_type_to_buffer(i32);
put_type_to_buffer(i64);
put_type_to_buffer(f32);
put_type_to_buffer(f64);

#define container_of_type_at(ptr, type, member)                                \
  ((type *)((uint8_t *)(ptr) - offsetof(type, member)))

typedef void *(*ALLOC_FUNC)(u64 size);
typedef void (*FREE_FUNC)(void *ptr);
typedef void *(*REALLOC_FUNC)(void *ptr, u64 new_size);

#define DEFER(fn) __attribute__((cleanup(fn)))

typedef struct MEMORY_ALLOCATOR_HEADER {
  ALLOC_FUNC alloc;
  FREE_FUNC free;
  REALLOC_FUNC realloc;
} MEMORY_ALLOCATOR_HEADER;

static MEMORY_ALLOCATOR_HEADER default_memory_allocator = {
    .alloc = (ALLOC_FUNC)malloc,
    .free = (FREE_FUNC)free,
    .realloc = (REALLOC_FUNC)realloc,
};