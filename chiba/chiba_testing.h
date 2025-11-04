#pragma once
#include "basic_memory.h"
#include <stdio.h>

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

#define TEST_CASE(name, group_name, desc, test)                                \
  int test_##name##_##group_name() {                                           \
    do {                                                                       \
      printf("%s%s-> Running Test: %s%s\n", ANSI_BOLD, ANSI_BLUE, #name,       \
             ANSI_RESET);                                                      \
    } while (0);                                                               \
    test                                                                       \
  }                                                                            \
  void register_test_##name##_##group_name() {                                 \
    int __test_id = test_##group_name##_count++;                               \
    tests_##group_name[__test_id] = test_##name##_##group_name;                \
    test_##group_name##_descriptions[__test_id] = desc;                        \
  }

#define DESC(name)                                                             \
  do {                                                                         \
    printf("%s%s-> Running Test: %s%s\n", ANSI_BOLD, ANSI_BLUE, #name,         \
           ANSI_RESET);                                                        \
  } while (0)

#define ASSERT_EQ(expected, actual, msg)                                       \
  do {                                                                         \
    if ((expected) != (actual)) {                                              \
      printf(                                                                  \
          "%s%s   Assertion Failed:%s %sExpected: %lld, Actual: %lld%s, At: "  \
          "%s:%d\n\t%s\n",                                                     \
          ANSI_BOLD, ANSI_RED, ANSI_RESET, ANSI_BOLD, (i64)(expected),         \
          (i64)(actual), ANSI_RESET, __FILE__, __LINE__, msg);                 \
      return -1;                                                               \
    } else {                                                                   \
      printf("%s%s   Assertion Passed: %s%s\n", ANSI_BOLD, ANSI_GREEN, msg,    \
             ANSI_RESET);                                                      \
    }                                                                          \
  } while (0)

#define ASSERT_TRUE(condition, msg)                                            \
  do {                                                                         \
    if (!(condition)) {                                                        \
      printf("%s%s   Assertion Failed:%s %sCondition is false%s\n\t%s\n",      \
             ANSI_BOLD, ANSI_RED, ANSI_RESET, ANSI_BOLD, ANSI_RESET, msg);     \
      return -1;                                                               \
    } else {                                                                   \
      printf("%s%s   Assertion Passed: %s%s\n", ANSI_BOLD, ANSI_GREEN, msg,    \
             ANSI_RESET);                                                      \
    }                                                                          \
  } while (0)

#define ASSERT_NOT_NULL(ptr, msg)                                              \
  do {                                                                         \
    if ((ptr) == NULL) {                                                       \
      printf("%s%s   Assertion Failed:%s %sPointer is NULL%s\n\t%s\n",         \
             ANSI_BOLD, ANSI_RED, ANSI_RESET, ANSI_BOLD, ANSI_RESET, msg);     \
      return -1;                                                               \
    } else {                                                                   \
      printf("%s%s   Assertion Passed: %s%s\n", ANSI_BOLD, ANSI_GREEN, msg,    \
             ANSI_RESET);                                                      \
    }                                                                          \
  } while (0)

#define ASSERT_NULL(ptr, msg)                                                  \
  do {                                                                         \
    if ((ptr) != NULL) {                                                       \
      printf("%s%s   Assertion Failed:%s %sPointer is not NULL%s\n\t%s\n",     \
             ANSI_BOLD, ANSI_RED, ANSI_RESET, ANSI_BOLD, ANSI_RESET, msg);     \
      return -1;                                                               \
    } else {                                                                   \
      printf("%s%s   Assertion Passed: %s%s\n", ANSI_BOLD, ANSI_GREEN, msg,    \
             ANSI_RESET);                                                      \
    }                                                                          \
  } while (0)

#define TEST_GROUP(name)                                                       \
  typedef int (*test_##name##_t)();                                            \
  test_##name##_t tests_##name[128] = {};                                      \
  static int test_##name##_count = 0;                                          \
  const char *test_##name##_descriptions[128] = {};                            \
  void register_all_tests_##name();

#define REGISTER_TEST(name, group_name) register_test_##name##_##group_name()
#define REGISTER_TEST_GROUP(group_name) void register_all_tests_##group_name()
#define ENABLE_TEST_GROUP(name)                                                \
  void register_all_tests_##name();                                            \
  int main() {                                                                 \
    register_all_tests_##name();                                               \
    int passed = 0;                                                            \
    int total = 0;                                                             \
    const char *failed[128] = {0};                                             \
    for (int i = 0; tests_##name[i] != NULL; i++) {                            \
      START_TIMER(test_timer_##name);                                          \
      int ret = tests_##name[i]();                                             \
      END_TIMER(test_timer_##name);                                            \
      printf("%s%s   Test %d Duration: %.6f seconds%s\n\n", ANSI_BOLD,         \
             ANSI_CYAN, i + 1, test_timer_##name##_elapsed, ANSI_RESET);       \
      total++;                                                                 \
      if (ret == 0) {                                                          \
        passed++;                                                              \
      } else {                                                                 \
        failed[i] = test_##name##_descriptions[i];                             \
      }                                                                        \
    }                                                                          \
    for (int i = 0; i < 128 && failed[i] != NULL; i++) {                       \
      printf("%s%s-> Test Failed: %s%s\n", ANSI_BOLD, ANSI_RED, failed[i],     \
             ANSI_RESET);                                                      \
    }                                                                          \
    printf("%s%s\n   --------------------\n   Test Summary: %d/%d tests "      \
           "passed.%s\n",                                                      \
           ANSI_BOLD, (passed == total) ? ANSI_GREEN : ANSI_RED, passed,       \
           total, ANSI_RESET);                                                 \
    return passed == total ? 0 : 1;                                            \
  }
