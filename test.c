#include "chiba/system/coroutine/coroutine.h"
#include <assert.h>

#define STKSZ 32768

char tstr[512];
char tresult[4096] = {0};

#define tprintf(...)                                                           \
  {                                                                            \
    printf(__VA_ARGS__);                                                       \
    snprintf(tstr, sizeof(tstr), __VA_ARGS__);                                 \
    strcat(tresult, tstr);                                                     \
  }

void cleanup(void *stk, u64 stksz, void *udata) {
  tprintf("(cleanup %d)\n", (int)(intptr_t)udata);
  free(stk);
}

struct CHIBA_co *co1, *co2, *co3;

void entry3(void *udata) {
  co3 = CHIBA_co_current();
  tprintf("(entry %d)\n", (int)(intptr_t)udata);
  CHIBA_co_switch(co1, false);
  tprintf("(mark C)\n");
  CHIBA_co_switch(co1, true);
}

void entry2(void *udata) {
  co2 = CHIBA_co_current();
  tprintf("(entry %d)\n", (int)(intptr_t)udata);
  struct CHIBA_co_desc desc = {
      .stack = malloc(STKSZ),
      .stack_size = STKSZ,
      .entry = entry3,
      .defer = cleanup,
      .ctx = (void *)3,
  };
  CHIBA_co_start(&desc, true);
}

void entry1(void *udata) {
  co1 = CHIBA_co_current();
  tprintf("(entry %d)\n", (int)(intptr_t)udata);
  struct CHIBA_co_desc desc = {
      .stack = malloc(STKSZ),
      .stack_size = STKSZ,
      .entry = entry2,
      .defer = cleanup,
      .ctx = (void *)2,
  };
  CHIBA_co_start(&desc, false);
  tprintf("(mark B)\n");
  CHIBA_co_switch(co3, false);
  tprintf("(mark D)\n");
  CHIBA_co_switch(0, true);
}

void perfcleanup(void *stk, u64 stksz, void *udata) { free(stk); }

// Return monotonic nanoseconds of the CPU clock.
static int64_t getnow(void) {
  struct timespec now = {0};
  clock_gettime(CLOCK_MONOTONIC, &now);
  return now.tv_sec * INT64_C(1000000000) + now.tv_nsec;
}

int64_t perfstart, perfend;

struct CHIBA_co *perf1co;
struct CHIBA_co *perf2co;

#define NSWITCHES 10000000
int perf_count = 0;

void perf1(void *udata) {
  perf1co = CHIBA_co_current();
  CHIBA_co_switch(0, 0);
  while (perf_count < NSWITCHES) {
    perf_count++;
    CHIBA_co_switch(perf2co, 0);
  }
  CHIBA_co_switch(0, 1);
}

void perf2(void *udata) {
  perf2co = CHIBA_co_current();
  perfstart = getnow();
  while (perf_count < NSWITCHES) {
    perf_count++;
    CHIBA_co_switch(perf1co, 0);
  }
  perfend = getnow();
  CHIBA_co_switch(perf1co, 1);
}

void test_perf() {
  struct CHIBA_co_desc desc1 = {
      .stack = malloc(STKSZ),
      .stack_size = STKSZ,
      .entry = perf1,
      .defer = perfcleanup,
      .ctx = (void *)1,
  };
  CHIBA_co_start(&desc1, false);

  struct CHIBA_co_desc desc2 = {
      .stack = malloc(STKSZ),
      .stack_size = STKSZ,
      .entry = perf2,
      .defer = perfcleanup,
      .ctx = (void *)2,
  };
  CHIBA_co_start(&desc2, false);

  printf("perf: %ld switches in %.3f secs, %ld ns / switch\n", (long)perf_count,
         (double)(perfend - perfstart) / 1e9,
         (long)((perfend - perfstart) / perf_count));
}

int main(void) {
  printf("%s\n", CHIBA_co_method(0));
  assert(!CHIBA_co_current());
  tprintf("(mark A)\n");
  struct CHIBA_co_desc desc = {
      .stack = malloc(STKSZ),
      .stack_size = STKSZ,
      .entry = entry1,
      .defer = cleanup,
      .ctx = (void *)1,
  };
  CHIBA_co_start(&desc, false);
  tprintf("(mark E)\n");
  assert(!CHIBA_co_current());
  const char *exp = "(mark A)\n"
                    "(entry 1)\n"
                    "(entry 2)\n"
                    "(cleanup 2)\n"
                    "(entry 3)\n"
                    "(mark B)\n"
                    "(mark C)\n"
                    "(cleanup 3)\n"
                    "(mark D)\n"
                    "(cleanup 1)\n"
                    "(mark E)\n";
  if (strcmp(tresult, exp) != 0) {
    printf("== expected ==\n%s", exp);
    printf("FAILED\n");
  } else {
    test_perf();
    printf("PASSED\n");
  }
  return 0;
}