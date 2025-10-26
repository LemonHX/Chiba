#pragma once

#include "chiba_utils_basic_types.h"
#include "chiba_utils_visibility.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define START_TIMER(timer_id)                                                  \
  LARGE_INTEGER timer_id##_start, timer_id##_end, timer_id##_freq;             \
  f64 timer_id##_elapsed;                                                      \
  QueryPerformanceFrequency(&timer_id##_freq);                                 \
  QueryPerformanceCounter(&timer_id##_start);
#define END_TIMER(timer_id)                                                    \
  QueryPerformanceCounter(&timer_id##_end);                                    \
  timer_id##_elapsed =                                                         \
      (f64)(timer_id##_end.QuadPart - timer_id##_start.QuadPart) /             \
      (f64)timer_id##_freq.QuadPart;
#else // POSIX
#include <time.h>
#define START_TIMER(timer_id)                                                  \
  struct timespec timer_id##_start, timer_id##_end;                            \
  f64 timer_id##_elapsed;                                                      \
  clock_gettime(CLOCK_MONOTONIC, &timer_id##_start);
#define END_TIMER(timer_id)                                                    \
  clock_gettime(CLOCK_MONOTONIC, &timer_id##_end);                             \
  timer_id##_elapsed =                                                         \
      (f64)(timer_id##_end.tv_sec - timer_id##_start.tv_sec) +                 \
      (f64)(timer_id##_end.tv_nsec - timer_id##_start.tv_nsec) / 1e9;
#endif

UTILS u64 get_time_in_nanoseconds();

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