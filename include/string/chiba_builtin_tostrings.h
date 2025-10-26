#pragma once

#include "../container/vec.h"
#include "../utils/chiba_utils.h"

C8Vec(u8) __builtin_str_to_string(MEMORY_ALLOCATOR_HEADER *allocator, str s) {
  C8Vec(u8) vec = NEW_VEC(u8, allocator);
  append_CHIBA_Vec_u8(&vec, (u8 *)s.data, s.len);
  return vec;
}

void itoa(int n, char *s) {
  int i, sign;

  if ((sign = n) < 0) /* record sign */
    n = -n;           /* make n positive */
  i = 0;

  do {                     /* generate digits in reverse order */
    s[i++] = n % 10 + '0'; /* get next digit */
  } while ((n /= 10) > 0); /* delete it */

  if (sign < 0)
    s[i++] = '-';

  reverse(s);
  s[i] = '\0';
  return;
}
C8Vec(u8)
    __builtin_u64_to_string(MEMORY_ALLOCATOR_HEADER *allocator, u64 value) {
  i8 buffer[21]; // max u64 is 20 digits + null terminator
  itoa(value, buffer, 10);
  str s = (str){.data = buffer, .len = strlen(buffer)};
  return __builtin_str_to_string(allocator, s);
}