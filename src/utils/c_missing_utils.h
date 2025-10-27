#pragma once
#include "../abi/basic_type.h"

#define DEFER(fn) __attribute__((cleanup(fn)))

// Determine system endianness
#ifndef is_be
UTILS u8 is_be(void) {
  union {
    u16 a;
    u8 b;
  } u = {0x100};
  return u.b;
}
#endif

// chiba does not support big-endian systems
BEFORE_START void abort_if_be(void) {
  if (is_be()) {
    fprintf(stderr, "Big-endian systems are not supported\n");
    abort();
  }
}


#ifndef offsetof
#define offsetof(type, field) ((size_t)&((type *)0)->field)
#endif

#define countof(x) (sizeof(x) / sizeof((x)[0]))
#define endof(x) ((x) + countof(x))
#define INF (1.0 / 0.0)
#define NEG_INF (-1.0 / 0.0)


#define max_of_type(type)                                                      \
  PRIVATE inline type max_##type(type a, type b) { return (a > b) ? a : b; }

#define min_of_type(type)                                                      \
  PRIVATE inline type min_##type(type a, type b) { return (a < b) ? a : b; }

max_of_type(u8);
max_of_type(u16);
max_of_type(u32);
max_of_type(u64);
max_of_type(i8);
max_of_type(i16);
max_of_type(i32);
max_of_type(i64);

min_of_type(u8);
min_of_type(u16);
min_of_type(u32);
min_of_type(u64);
min_of_type(i8);
min_of_type(i16);
min_of_type(i32);
min_of_type(i64);

/* WARNING: undefined if a = 0 */
PRIVATE inline i32 clz32(u32 a) {
#if defined(_MSC_VER) && !defined(__clang__)
  unsigned long index;
  _BitScanReverse(&index, a);
  return 31 - index;
#else
  return __builtin_clz(a);
#endif
}

/* WARNING: undefined if a = 0 */
UTILS i32 clz64(u64 a) {
#if defined(_MSC_VER) && !defined(__clang__)
#if INTPTR_MAX == INT64_MAX
  unsigned long index;
  _BitScanReverse64(&index, a);
  return 63 - index;
#else
  if (a >> 32)
    return clz32((unsigned)(a >> 32));
  else
    return clz32((unsigned)a) + 32;
#endif
#else
  return __builtin_clzll(a);
#endif
}

/* WARNING: undefined if a = 0 */
UTILS i32 ctz32(u32 a) {
#if defined(_MSC_VER) && !defined(__clang__)
  unsigned long index;
  _BitScanForward(&index, a);
  return index;
#else
  return __builtin_ctz(a);
#endif
}

/* WARNING: undefined if a = 0 */
UTILS i32 ctz64(u64 a) {
#if defined(_MSC_VER) && !defined(__clang__)
  unsigned long index;
  _BitScanForward64(&index, a);
  return index;
#else
  return __builtin_ctzll(a);
#endif
}

UTILS u64 f64_to_u64(f64 d) {
  union {
    f64 d;
    u64 u;
  } u;
  u.d = d;
  return u.u;
}

UTILS f64 u64_to_f64(u64 uu) {
  union {
    f64 d;
    u64 u;
  } u;
  u.u = uu;
  return u.d;
}

#ifndef bswap16
PRIVATE inline u16 bswap16(u16 x) { return (x >> 8) | (x << 8); }
#endif

#ifndef bswap32
PRIVATE inline u32 bswap32(u32 v) {
  return ((v & 0xff000000) >> 24) | ((v & 0x00ff0000) >> 8) |
         ((v & 0x0000ff00) << 8) | ((v & 0x000000ff) << 24);
}
#endif

#ifndef bswap64
PRIVATE inline u64 bswap64(u64 v) {
  return ((v & ((u64)0xff << (7 * 8))) >> (7 * 8)) |
         ((v & ((u64)0xff << (6 * 8))) >> (5 * 8)) |
         ((v & ((u64)0xff << (5 * 8))) >> (3 * 8)) |
         ((v & ((u64)0xff << (4 * 8))) >> (1 * 8)) |
         ((v & ((u64)0xff << (3 * 8))) << (1 * 8)) |
         ((v & ((u64)0xff << (2 * 8))) << (3 * 8)) |
         ((v & ((u64)0xff << (1 * 8))) << (5 * 8)) |
         ((v & ((u64)0xff << (0 * 8))) << (7 * 8));
}
#endif

// ========== TIME ===========

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
UTILS u64 get_time_in_nanoseconds() {
  LARGE_INTEGER freq, counter;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&counter);
  return (u64)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
}
#else // POSIX
#include <time.h>
UTILS u64 get_time_in_nanoseconds() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
}
#endif

// ========== BUFFER ==========

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

// ========== TO_STRING ==========

#include <string.h>

// 数字查找表：每两个字节表示一个两位数 "00" "01" ... "99"
PRIVATE const u8 DEC_DIGITS_LUT[200] = {
    '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0',
    '7', '0', '8', '0', '9', '1', '0', '1', '1', '1', '2', '1', '3', '1', '4',
    '1', '5', '1', '6', '1', '7', '1', '8', '1', '9', '2', '0', '2', '1', '2',
    '2', '2', '3', '2', '4', '2', '5', '2', '6', '2', '7', '2', '8', '2', '9',
    '3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5', '3', '6', '3',
    '7', '3', '8', '3', '9', '4', '0', '4', '1', '4', '2', '4', '3', '4', '4',
    '4', '5', '4', '6', '4', '7', '4', '8', '4', '9', '5', '0', '5', '1', '5',
    '2', '5', '3', '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9',
    '6', '0', '6', '1', '6', '2', '6', '3', '6', '4', '6', '5', '6', '6', '6',
    '7', '6', '8', '6', '9', '7', '0', '7', '1', '7', '2', '7', '3', '7', '4',
    '7', '5', '7', '6', '7', '7', '7', '8', '7', '9', '8', '0', '8', '1', '8',
    '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
    '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9',
    '7', '9', '8', '9', '9'};

PRIVATE const u64 MAX_STR_LEN = 20;

UTILS cstr itoa(i64 value, i8 *buffer) {
  const bool is_nonnegative = value >= 0;
  u64 n = is_nonnegative ? (u64)(value) : (u64)(-(value + 1)) + 1;

  u64 curr = MAX_STR_LEN;
  const u8 *lut_ptr = DEC_DIGITS_LUT;

  // Render 4 digits at a time
  while (n >= 10000) {
    const u64 rem = n % 10000;
    n /= 10000;

    const u64 d1 = (rem / 100) << 1;
    const u64 d2 = (rem % 100) << 1;
    curr -= 4;
    memcpy(buffer + curr, lut_ptr + d1, 2);
    memcpy(buffer + curr + 2, lut_ptr + d2, 2);
  }

  // Render 2 more digits, if >= 100
  if (n >= 100) {
    const u64 d1 = (n % 100) << 1;
    n /= 100;
    curr -= 2;
    memcpy(buffer + curr, lut_ptr + d1, 2);
  }

  // Render last 1 or 2 digits
  if (n < 10) {
    curr -= 1;
    buffer[curr] = (i8)(n) + '0';
  } else {
    const u64 d1 = n << 1;
    curr -= 2;
    memcpy(buffer + curr, lut_ptr + d1, 2);
  }

  if (!is_nonnegative) {
    curr -= 1;
    buffer[curr] = '-';
  }

  return buffer + curr;
}

UTILS cstr utoa(u64 value, i8 *buffer) {
  u64 n = value;

  u64 curr = MAX_STR_LEN;
  const u8 *lut_ptr = DEC_DIGITS_LUT;

  // Render 4 digits at a time
  while (n >= 10000) {
    const u64 rem = n % 10000;
    n /= 10000;

    const u64 d1 = (rem / 100) << 1;
    const u64 d2 = (rem % 100) << 1;
    curr -= 4;
    memcpy(buffer + curr, lut_ptr + d1, 2);
    memcpy(buffer + curr + 2, lut_ptr + d2, 2);
  }

  // Render 2 more digits, if >= 100
  if (n >= 100) {
    const u64 d1 = (n % 100) << 1;
    n /= 100;
    curr -= 2;
    memcpy(buffer + curr, lut_ptr + d1, 2);
  }

  // Render last 1 or 2 digits
  if (n < 10) {
    curr -= 1;
    buffer[curr] = (i8)(n) + '0';
  } else {
    const u64 d1 = n << 1;
    curr -= 2;
    memcpy(buffer + curr, lut_ptr + d1, 2);
  }

  return buffer + curr;
}

UTILS cstr ftoa(f32 value, i8 *buffer) {
#warning TODO: implement ftoa
  sprintf((char *)buffer, "%f", value);
  return buffer;
}

UTILS cstr dtoa(f64 value, i8 *buffer) {
#warning TODO: implement dtoa
  sprintf((char *)buffer, "%lf", value);
  return buffer;
}

UTILS cstr ptrtoa(void *ptr, i8 *buffer) {
#warning TODO: implement ptrtoa
  sprintf((char *)buffer, "%p", ptr);
  return buffer;
}


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