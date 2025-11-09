// Chiba runtime uses nan_boxing for basic types

#pragma once
#include "common_headers.h"

#ifdef __BIG_ENDIAN__ // is big endian
#error "Big endian architecture is not supported"
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

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
UTILS u64 get_time_in_nanoseconds() {
  LARGE_INTEGER freq, counter;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&counter);
  return (u64)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
}
#elif !defined(__EMSCRIPTEN__) // POSIX
#include <time.h>
UTILS u64 get_time_in_nanoseconds() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (u64)ts.tv_sec * 1000000000ULL + (u64)ts.tv_nsec;
}
#else
UTILS u64 get_time_in_nanoseconds() {
  // emscripten_get_now() 返回毫秒，转换为纳秒
  return (u64)(emscripten_get_now() * 1000000.0);
}
#endif