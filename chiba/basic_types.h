// Chiba runtime uses nan_boxing for basic types

#pragma once
#include "common_headers.h"

#ifdef __BIG_ENDIAN__ // is big endian
#error "Big endian architecture is not supported"
#endif

#ifdef __LITTLE_ENDIAN__
#pragma message("INFO: Little endian architecture detected")
#endif

#define true 1
#define false 0
#define bool u8
typedef unsigned char u8;
typedef u8 bool;
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

// Forward declaration of entry function that will be implemented in the main
// coroutine code
NOINLINE PRIVATE void CHIBA_co_entry(anyptr arg) __attribute__((noreturn));