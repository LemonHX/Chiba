// Rewrite tests to mirror test.c semantics more closely
#include "coroutine.h"
#include "../chiba_testing.h"
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
#include <sys/wait.h>
#include <unistd.h>
#endif

#define STKSZ 32768

// Accumulated output buffer and helper
static char tstr[256];
static char tresult[4096];
#define tprintf(...)                                                           \
  do {                                                                         \
    snprintf(tstr, sizeof(tstr), __VA_ARGS__);                                 \
    strcat(tresult, tstr);                                                     \
  } while (0)

TEST_GROUP(coroutine);

// ---------------- Sequence/cleanup order scenario ----------------
static struct chiba_co *co1 = NULL;
static struct chiba_co *co2 = NULL;
static struct chiba_co *co3 = NULL;

static void test_cleanup(anyptr stk, u64 stksz, anyptr udata) {
  (void)stksz;
  tprintf("(cleanup %d)\n", (int)(intptr_t)udata);
  CHIBA_INTERNAL_free(stk);
}
// 6. invalid_params_basic: verify abort on invalid desc and small stack using
// fork (POSIX only)
#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
static int run_invalid_start(anyptr d) {
  (void)d;
  struct chiba_co_desc bad = {0};
  chiba_co_start(&bad, false);
  return 0;
}
static int run_null_start() {
  chiba_co_start(NULL, false);
  return 0;
}
TEST_CASE(invalid_params_basic, coroutine, "invalid parameters trigger abort", {
  DESC(invalid_params_basic);
  // fork to capture abort without killing test harness
  pid_t pid = fork();
  ASSERT_TRUE(pid >= 0, "fork succeeded");
  if (pid == 0) { // child
    run_null_start();
    _Exit(0);
  }
  int status = 0;
  waitpid(pid, &status, 0);
  ASSERT_TRUE(WIFSIGNALED(status) || WIFEXITED(status), "child exited");
  ASSERT_TRUE(WIFSIGNALED(status), "null desc caused abort (signal)");
  pid = fork();
  ASSERT_TRUE(pid >= 0, "fork2 succeeded");
  if (pid == 0) {
    run_invalid_start(NULL);
    _Exit(0);
  }
  status = 0;
  waitpid(pid, &status, 0);
  ASSERT_TRUE(WIFSIGNALED(status), "small stack abort");
  return 0;
})
#else
TEST_CASE(invalid_params_basic, coroutine, "invalid parameters (skipped)", {
  DESC(invalid_params_basic);
  return 0;
})
#endif

// 8. heavy_switch_pressure: large number of switches with minimal work
static struct chiba_co *hs_a = NULL, *hs_b = NULL;
static long hs_cnt = 0;
static long hs_target = 1000000;
static void hs_cleanup(anyptr s, u64 n, anyptr u) {
  (void)n;
  (void)u;
  CHIBA_INTERNAL_free(s);
}
static void hs_entry_a(anyptr u) {
  (void)u;
  hs_a = chiba_co_current();
  chiba_co_switch(NULL, false);
  while (hs_cnt < hs_target) {
    hs_cnt++;
    chiba_co_switch(hs_b, false);
  }
  chiba_co_switch(NULL, true);
}
static void hs_entry_b(anyptr u) {
  (void)u;
  hs_b = chiba_co_current();
  while (hs_cnt < hs_target) {
    hs_cnt++;
    chiba_co_switch(hs_a, false);
  }
  chiba_co_switch(hs_a, true);
}
TEST_CASE(heavy_switch_pressure, coroutine, "heavy switch pressure test", {
  DESC(heavy_switch_pressure);
  hs_cnt = 0;
  hs_a = hs_b = NULL;
  struct chiba_co_desc a;
  a.stack = CHIBA_INTERNAL_malloc(STKSZ);
  a.stack_size = STKSZ;
  a.entry = hs_entry_a;
  a.defer = hs_cleanup;
  a.ctx = NULL;
  struct chiba_co_desc b;
  b.stack = CHIBA_INTERNAL_malloc(STKSZ);
  b.stack_size = STKSZ;
  b.entry = hs_entry_b;
  b.defer = hs_cleanup;
  b.ctx = NULL;
  chiba_co_start(&a, false);
  chiba_co_start(&b, false);
  ASSERT_EQ(hs_target, hs_cnt, "performed expected heavy switches");
  return 0;
})

static void entry3(anyptr udata) {
  co3 = chiba_co_current();
  tprintf("(entry %d)\n", (int)(intptr_t)udata);
  chiba_co_switch(co1, false);
  tprintf("(mark C)\n");
  chiba_co_switch(co1, true);
}

static void entry2(anyptr udata) {
  co2 = chiba_co_current();
  tprintf("(entry %d)\n", (int)(intptr_t)udata);
  struct chiba_co_desc desc = {
      .stack = CHIBA_INTERNAL_malloc(STKSZ),
      .stack_size = STKSZ,
      .entry = entry3,
      .defer = test_cleanup,
      .ctx = (anyptr)3,
  };
  chiba_co_start(&desc, true);
}

static void entry1(anyptr udata) {
  co1 = chiba_co_current();
  tprintf("(entry %d)\n", (int)(intptr_t)udata);
  struct chiba_co_desc desc = {
      .stack = CHIBA_INTERNAL_malloc(STKSZ),
      .stack_size = STKSZ,
      .entry = entry2,
      .defer = test_cleanup,
      .ctx = (anyptr)2,
  };
  chiba_co_start(&desc, false);
  tprintf("(mark B)\n");
  chiba_co_switch(co3, false);
  tprintf("(mark D)\n");
  chiba_co_switch(NULL, true);
}

TEST_CASE(
    sequence_and_cleanup_order, coroutine,
    "Sequence and cleanup order like test.c", {
      DESC(sequence_and_cleanup_order);

      // Reset buffer
      tresult[0] = '\0';

      ASSERT_NULL(chiba_co_current(), "Not in coroutine initially");
      tprintf("(mark A)\n");

      struct chiba_co_desc desc; /* avoid designated initializer in macro */
      desc.stack = CHIBA_INTERNAL_malloc(STKSZ);
      desc.stack_size = STKSZ;
      desc.entry = entry1;
      desc.defer = test_cleanup;
      desc.ctx = (anyptr)1;
      chiba_co_start(&desc, false);

      tprintf("(mark E)\n");

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

      ASSERT_TRUE(strcmp(tresult, exp) == 0,
                  "Output sequence matches expected");
      return 0;
    })

// ---------------- Light performance sanity (reduced switches) ---------------
static struct chiba_co *p1 = NULL, *p2 = NULL;
static int pcount = 0;
static const int PN = 200000; // keep runtime reasonable

static void pcleanup(anyptr s, u64 n, anyptr u) {
  (void)n;
  (void)u;
  CHIBA_INTERNAL_free(s);
}

static void perf1(anyptr u) {
  (void)u;
  p1 = chiba_co_current();
  chiba_co_switch(NULL, false);
  while (pcount < PN) {
    pcount++;
    chiba_co_switch(p2, false);
  }
  chiba_co_switch(NULL, true);
}

static void perf2(anyptr u) {
  (void)u;
  p2 = chiba_co_current();
  while (pcount < PN) {
    pcount++;
    chiba_co_switch(p1, false);
  }
  chiba_co_switch(p1, true);
}

TEST_CASE(performance_switching, coroutine, "Light performance sanity", {
  DESC(performance_switching);
  pcount = 0;
  p1 = p2 = NULL;
  struct chiba_co_desc d1; /* avoid designated initializer in macro */
  d1.stack = CHIBA_INTERNAL_malloc(STKSZ);
  d1.stack_size = STKSZ;
  d1.entry = perf1;
  d1.defer = pcleanup;
  d1.ctx = (anyptr)1;
  struct chiba_co_desc d2; /* avoid designated initializer in macro */
  d2.stack = CHIBA_INTERNAL_malloc(STKSZ);
  d2.stack_size = STKSZ;
  d2.entry = perf2;
  d2.defer = pcleanup;
  d2.ctx = (anyptr)2;
  chiba_co_start(&d1, false);
  chiba_co_start(&d2, false);
  ASSERT_EQ(PN, pcount, "Performed expected number of switches");
  return 0;
})

// ---------------- Additional Tests ----------------
// 1. chiba_co_method string non-empty and contains comma
TEST_CASE(method_string, coroutine, "co_method returns expected format", {
  DESC(method_string);
  cstr m = chiba_co_method(NULL);
  ASSERT_NOT_NULL(m, "method string not null");
  ASSERT_TRUE(strlen(m) > 0, "method string non-empty");
  ASSERT_TRUE(strchr(m, ',') != NULL || strchr(m, ' ') != NULL,
              "method string contains separator");
  return 0;
})

// 2. current pointer validity: inside coroutine chiba_co_current stable
static struct chiba_co *cur_stable_a = NULL, *cur_stable_b = NULL,
                       *cur_helper = NULL;
static int cur_phase = 0;
static void cur_entry_primary(anyptr u) {
  (void)u;
  cur_stable_a = chiba_co_current();
  cur_phase = 1;
  chiba_co_switch(cur_helper, false);
  cur_phase = 2;
  chiba_co_switch(NULL, true);
}
static void cur_entry_helper(anyptr u) {
  (void)u;
  cur_helper = chiba_co_current();
  cur_stable_b = cur_helper;
  chiba_co_switch(cur_stable_a, false);
  chiba_co_switch(NULL, true);
}
TEST_CASE(current_pointer_validity, coroutine, "current pointer stability", {
  DESC(current_pointer_validity);
  ASSERT_NULL(chiba_co_current(), "outside coroutine null");
  cur_phase = 0;
  cur_stable_a = cur_stable_b = cur_helper = NULL;
  struct chiba_co_desc da;
  da.stack = CHIBA_INTERNAL_malloc(STKSZ);
  da.stack_size = STKSZ;
  da.entry = cur_entry_primary;
  da.defer = pcleanup;
  da.ctx = NULL;
  struct chiba_co_desc db;
  db.stack = CHIBA_INTERNAL_malloc(STKSZ);
  db.stack_size = STKSZ;
  db.entry = cur_entry_helper;
  db.defer = pcleanup;
  db.ctx = NULL;
  chiba_co_start(&da, false);
  chiba_co_start(&db, false);
  ASSERT_NOT_NULL(cur_stable_a, "primary captured");
  ASSERT_NOT_NULL(cur_stable_b, "helper captured");
  ASSERT_TRUE(cur_stable_a != cur_stable_b, "different coroutine pointers");
  ASSERT_EQ(2, cur_phase, "primary progressed to phase 2 before final");
  ASSERT_NULL(chiba_co_current(), "after final switches null again");
  return 0;
})

// 3. final_switch_cleanup_order: ensure final=true on switch schedules cleanup
// exactly once
static int fs_cleanup_calls = 0;
static int fs_step = 0;
static struct chiba_co *fs_co = NULL;
static void fs_cleanup(anyptr s, u64 n, anyptr u) {
  (void)n;
  (void)u;
  fs_cleanup_calls++;
  CHIBA_INTERNAL_free(s);
}
static void fs_entry(anyptr u) {
  (void)u;
  fs_co = chiba_co_current();
  fs_step = 1;
  chiba_co_switch(NULL,
                  true); // final switch immediately; code after not executed
  fs_step = 99;          // unreachable if implementation correct
}
TEST_CASE(final_switch_cleanup_order, coroutine,
          "final switch triggers cleanup once", {
            DESC(final_switch_cleanup_order);
            fs_cleanup_calls = 0;
            fs_step = 0;
            fs_co = NULL;
            struct chiba_co_desc d;
            d.stack = CHIBA_INTERNAL_malloc(STKSZ);
            d.stack_size = STKSZ;
            d.entry = fs_entry;
            d.defer = fs_cleanup;
            d.ctx = NULL;
            chiba_co_start(&d, false);
            ASSERT_EQ(1, fs_step,
                      "fs_step remained 1 (no post-final execution)");
            ASSERT_EQ(1, fs_cleanup_calls, "cleanup called exactly once");
            return 0;
          })

// 4. self_final_no_cleanup: calling final switch to same coroutine should not
// double cleanup
static int self_final_calls = 0;
static struct chiba_co *self_co = NULL;
static void self_cleanup(anyptr s, u64 n, anyptr u) {
  (void)n;
  (void)u;
  self_final_calls++;
  CHIBA_INTERNAL_free(s);
}
static void self_entry(anyptr u) {
  (void)u;
  self_co = chiba_co_current();    // Switch to self with final=false
  chiba_co_switch(self_co, false); // no-op
  chiba_co_switch(self_co, true);  // final self-switch should be treated like
                                   // no-op & not schedule cleanup again
  chiba_co_switch(NULL, true);
}
TEST_CASE(self_final_no_cleanup, coroutine,
          "self final switch does not double cleanup", {
            DESC(self_final_no_cleanup);
            self_final_calls = 0;
            self_co = NULL;
            struct chiba_co_desc d;
            d.stack = CHIBA_INTERNAL_malloc(STKSZ);
            d.stack_size = STKSZ;
            d.entry = self_entry;
            d.defer = self_cleanup;
            d.ctx = NULL;
            chiba_co_start(&d, false);
            ASSERT_EQ(1, self_final_calls,
                      "cleanup called exactly once for self final scenario");
            return 0;
          })

// 5. child_final_start_cleans_parent: starting a child with final=true should
// cleanup parent before child executes entry body in next resume
static int cf_cleanup_parent = 0, cf_cleanup_child = 0;
static struct chiba_co *cf_parent = NULL;
static struct chiba_co *cf_child = NULL;
static int cf_phase = 0;
static void cf_parent_cleanup(anyptr s, u64 n, anyptr u) {
  (void)n;
  (void)u;
  cf_cleanup_parent++;
  CHIBA_INTERNAL_free(s);
}
static void cf_child_cleanup(anyptr s, u64 n, anyptr u) {
  (void)n;
  (void)u;
  cf_cleanup_child++;
  CHIBA_INTERNAL_free(s);
}
static void cf_child_entry(anyptr u) {
  (void)u;
  cf_child = chiba_co_current();
  cf_phase = 3;
  chiba_co_switch(NULL, true);
}
static void cf_parent_entry(anyptr u) {
  (void)u;
  cf_parent = chiba_co_current();
  cf_phase = 1;
  struct chiba_co_desc d;
  d.stack = CHIBA_INTERNAL_malloc(STKSZ);
  d.stack_size = STKSZ;
  d.entry = cf_child_entry;
  d.defer = cf_child_cleanup;
  d.ctx = NULL;
  chiba_co_start(&d, true);
  cf_phase = 2;
  chiba_co_switch(cf_child, false);
}
TEST_CASE(child_final_start_cleans_parent, coroutine,
          "child final start cleans parent first", {
            DESC(child_final_start_cleans_parent);
            cf_cleanup_parent = 0;
            cf_cleanup_child = 0;
            cf_parent = NULL;
            cf_child = NULL;
            cf_phase = 0;
            struct chiba_co_desc d;
            d.stack = CHIBA_INTERNAL_malloc(STKSZ);
            d.stack_size = STKSZ;
            d.entry = cf_parent_entry;
            d.defer = cf_parent_cleanup;
            d.ctx = NULL;
            chiba_co_start(&d, false);
            ASSERT_EQ(3, cf_phase, "phase reached 3 after child entry");
            ASSERT_EQ(1, cf_cleanup_parent, "parent cleaned once");
            ASSERT_EQ(1, cf_cleanup_child, "child cleaned once");
            return 0;
          })

// Register
REGISTER_TEST_GROUP(coroutine) {
  REGISTER_TEST(sequence_and_cleanup_order, coroutine);
  REGISTER_TEST(method_string, coroutine);
  REGISTER_TEST(current_pointer_validity, coroutine);
  REGISTER_TEST(final_switch_cleanup_order, coroutine);
  REGISTER_TEST(self_final_no_cleanup, coroutine);
  REGISTER_TEST(child_final_start_cleans_parent, coroutine);
  REGISTER_TEST(invalid_params_basic, coroutine);
  // heavy_switch_pressure: test heavy switching with minimal work
  REGISTER_TEST(heavy_switch_pressure, coroutine);
  REGISTER_TEST(performance_switching, coroutine);
}

ENABLE_TEST_GROUP(coroutine)
