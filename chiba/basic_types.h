// Chiba runtime uses nan_boxing for basic types

#pragma once
#include "common_headers.h"

#ifdef __BIG_ENDIAN__ // is big endian
#error "Big endian architecture is not supported"
#endif

#ifdef __LITTLE_ENDIAN__
#define CHIBA_NANBOX_LITTLE_ENDIAN
#endif

#ifndef __cplusplus
#define true 1
#define false 0
typedef unsigned char bool;
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u48;

typedef char i8;
typedef short i16;
typedef int i32;
typedef long long i48;

typedef float f32;
typedef char *cstr;

// only using 48bit pointer
typedef void *anyptr;

typedef double f64;

// need box to pass
typedef long long i64;
typedef unsigned long long u64;

// In little-endian, bit fields are laid out from LSB to MSB
// IEEE 754 double layout (64 bits):
// [sign(1) | exponent(11) | mantissa(52)]
// For NaN boxing: [sign(1) | exponent(11) | quiet(1) | tag(3) | payload(48)]
typedef struct __attribute__((packed)) CHIBA_NanBoxedLayout {
  u64 payload : 48;  // bits 0-47: 48 位有效载荷（指针或数据）
  u64 tag : 3;       // bits 48-50: 3 位类型标签
  u64 quiet : 1;     // bit 51: 1 位 Quiet NaN 标志
  u64 exponent : 11; // bits 52-62: 11 位指数（全1表示NaN）
  u64 sign : 1;      // bit 63: 1 位符号位
} CHIBA_NanBoxedLayout;

typedef struct __attribute__((packed)) CHIBA_NanBoxedUnsafePtr {
  u64 address : 48;
  u64 padding : 16;
} CHIBA_NanBoxedUnsafePtr;

enum TAG {
  TAG_anyptr = 0b000,
  TAG_u32 = 0b001,
  TAG_u48 = 0b010,
  TAG_f32 = 0b011,

  TAG_i32 = 0b101,
  TAG_i48 = 0b110,

  TAG_address = 0b100,
  TAG_object = 0b111,

  TAG_INVALID
};

typedef union CHIBA_NanBox {
  u64 abi;
  f64 f64;

  CHIBA_NanBoxedLayout layout;
  CHIBA_NanBoxedUnsafePtr ptr;
} CHIBA_NanBox;

UTILS bool CHIBA_assert_is_nanbox(f64 f64) {
  CHIBA_NanBox box;
  box.f64 = f64;
  u64 raw = box.abi;
  // 1. 检查是不是真的NaN
  bool is_nan = ((raw >> 52) & 0x7FF) == 0x7FF && ((raw >> 51) & 0x1) == 1;
  // 2. 检查是不是INF之类的
  bool is_inf = ((raw >> 52) & 0x7FF) == 0x7FF && ((raw >> 51) & 0x1) == 0;
  bool is_neg_inf = is_inf && ((raw >> 63) & 0x1) == 1;
  if (likely(is_nan && !is_inf && !is_neg_inf)) {
    return true;
  }
  return false;
}

UTILS u8 CHIBA_nanbox_get_tag(f64 f64) {
  CHIBA_NanBox box;
  box.f64 = f64;
  if (likely(CHIBA_assert_is_nanbox(f64))) {
    return box.layout.tag;
  }
  return TAG_INVALID;
}
UTILS u64 CHIBA_ptr_to_nanbox(anyptr ptr) {
  CHIBA_NanBox box;
  box.abi = 0;
  box.layout.exponent = 0x7FF;  // Set exponent to all 1s for NaN
  box.layout.quiet = 1;         // Set quiet NaN bit
  box.layout.tag = TAG_address; // Set tag to address type
  // FIXME: check if this works in rv64 39bit mode
  box.layout.payload = (u64)ptr & ((1ULL << PTR_BIT) - 1); // Mask to 48 bits
  return box.abi;
}

UTILS anyptr CHIBA_nanbox_to_ptr(f64 f64) {
  CHIBA_NanBox box;
  box.f64 = f64;
  if (likely(CHIBA_assert_is_nanbox(f64))) {
    if (likely(box.layout.tag == TAG_address)) {
      return (anyptr)box.layout.payload;
    }
  }
  return NULL;
}
