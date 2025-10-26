#pragma once
#include "chiba_utils_basic_types.h"
#include "chiba_utils_visibility.h"
#include <string.h>

// 数字查找表：每两个字节表示一个两位数 "00" "01" ... "99"
static const u8 DEC_DIGITS_LUT[200] = {
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

static const u64 MAX_STR_LEN = 20;

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
