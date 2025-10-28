#pragma once

#include "../common_headers.h"

#ifdef bool
#undef bool
#endif

#ifndef __cplusplus
typedef char bool;
#define true 1
#define false 0
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
typedef char *cstr;
typedef void *anyptr;

#ifndef NULL
#define NULL ((void *)0)
#endif

#define C8STRUCT struct __attribute__((aligned(8)))

// for compatibility, use cstr as inner string type
typedef C8STRUCT str {
  cstr data;
  u64 len;
}
str;

#define STR(cstr) ((str){.data = cstr, .len = sizeof(cstr) - 1})
#define PTR2STR(cbuffer) ((str){.data = cbuffer, .len = strlen(cbuffer)})
