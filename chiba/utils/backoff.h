#pragma once
#include "../basic_types.h"
#include "../common_headers.h"
#include <sched.h>

#define SPIN_LIMIT 6
#define YIELD_LIMIT 10

typedef struct {
  u32 step;
} chiba_backoff;

// Spin for a number of iterations
UTILS void backoff_spin(chiba_backoff *backoff) {
  u32 limit = backoff->step < SPIN_LIMIT ? backoff->step : SPIN_LIMIT;
  for (u32 i = 0; i < (1u << limit); i++) {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
    __asm__ __volatile__("yield" ::: "memory");
#elif defined(__riscv)
    __asm__ __volatile__("pause" ::: "memory");
#else
    // Generic spin hint
    __asm__ __volatile__("" ::: "memory");
#endif
  }
  if (backoff->step <= SPIN_LIMIT) {
    backoff->step++;
  }
}

// Snooze by spinning or yielding
UTILS void backoff_snooze(chiba_backoff *backoff) {
  if (backoff->step <= SPIN_LIMIT) {
    for (u32 i = 0; i < (1u << backoff->step); i++) {
#if defined(__x86_64__) || defined(__i386__)
      __builtin_ia32_pause();
#elif defined(__aarch64__)
      __asm__ __volatile__("yield" ::: "memory");
#elif defined(__riscv)
      __asm__ __volatile__("pause" ::: "memory");
#else
      __asm__ __volatile__("" ::: "memory");
#endif
    }
  } else {
    sched_yield();
  }
  if (backoff->step <= YIELD_LIMIT) {
    backoff->step++;
  }
}

// Check if backoff has completed all retry attempts
UTILS bool backoff_is_completed(const chiba_backoff *backoff) {
  return backoff->step > YIELD_LIMIT;
}
