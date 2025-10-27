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
get_type_from_buffer(anyptr);

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
put_type_to_buffer(anyptr);

#define DEFER(fn) __attribute__((cleanup(fn)))


#define container_of_type_at(ptr, type, member)                                \
  ((type *)((u8 *)(ptr) - offsetof(type, member)))



typedef void *(*ALLOC_FUNC)(u64 size);
typedef void (*FREE_FUNC)(void *ptr);
typedef void *(*REALLOC_FUNC)(void *ptr, u64 new_size);


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

UTILS bool str_equals(str a, str b) {
  if (a.len != b.len)
    return 0;
  return memcmp(a.data, b.data, a.len) == 0;
}

// using murmurhash3 algorithm
UTILS u32 cstr_hash(cstr key, u32 len, u32 seed) {
  u32 c1 = 0xcc9e2d51;
  u32 c2 = 0x1b873593;
  u32 r1 = 15;
  u32 r2 = 13;
  u32 m = 5;
  u32 n = 0xe6546b64;
  u32 h = 0;
  u32 k = 0;
  u8 *d = (u8 *)key; // 32 bit extract from `key'
  const u32 *chunks = NULL;
  const u8 *tail = NULL; // tail - last 8 bytes
  int i = 0;
  int l = len / 4; // chunk length

  h = seed;

  chunks = (const u32 *)(d + l * 4); // body
  tail = (const u8 *)(d + l * 4);    // last 8 byte chunk of `key'

  // for each 4 byte chunk of `key'
  for (i = -l; i != 0; ++i) {
    // next 4 byte chunk of `key'
    k = chunks[i];

    // encode next 4 byte chunk of `key'
    k *= c1;
    k = (k << r1) | (k >> (32 - r1));
    k *= c2;

    // append to hash
    h ^= k;
    h = (h << r2) | (h >> (32 - r2));
    h = h * m + n;
  }

  k = 0;

  // remainder
  switch (len & 3) { // `len % 4'
  case 3:
    k ^= (tail[2] << 16);
  case 2:
    k ^= (tail[1] << 8);

  case 1:
    k ^= tail[0];
    k *= c1;
    k = (k << r1) | (k >> (32 - r1));
    k *= c2;
    h ^= k;
  }

  h ^= len;

  h ^= (h >> 16);
  h *= 0x85ebca6b;
  h ^= (h >> 13);
  h *= 0xc2b2ae35;
  h ^= (h >> 16);

  return h;
}

const u32 INTERNAL_MURMURHASH3_SEED = 0xc815beaf;

UTILS u32 _internal_str_hash(str s) {
  return cstr_hash((cstr)s.data, (u32)s.len, INTERNAL_MURMURHASH3_SEED);
}

typedef u32 Symbol;

#define Symbol(cstr) _internal_str_hash(STR(cstr))