#pragma once

#include "chiba_utils_common_headers.h"

#ifdef bool
#undef bool
#endif

#ifndef __cplusplus
typedef char bool;
#endif
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef float f32;
typedef double f64;

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef struct str {
  i8 *data;
  u64 len;
} str;

#define STR(sstr) ((str){.data = (i8 *)(sstr), .len = sizeof(sstr) - 1})