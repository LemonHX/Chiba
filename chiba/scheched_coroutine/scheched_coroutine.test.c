// Port of sco/tests/test_sco.c semantics to chiba_scheched_coroutine using
// chiba_testing
#include "scheched_coroutine.h"
#include "../chiba_testing.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#define STKSZ 32768
#define NCHILDREN 100

TEST_GROUP(scheched_coroutine);

// Allocator helpers (track allocations like tests.h)
static atomic_size_t total_allocs = 0;
static atomic_size_t total_mem = 0;
PRIVATE void *xmalloc(size_t size) {
  void *mem = CHIBA_INTERNAL_malloc(size);
  assert(mem != NULL);
  atomic_fetch_add(&total_allocs, 1);
  return mem;
}
PRIVATE void xfree(void *mem) {
  if (mem) {
    CHIBA_INTERNAL_free(mem);
    atomic_fetch_sub(&total_allocs, 1);
  }
}

PRIVATE THREAD_LOCAL i32 started = 0;
PRIVATE THREAD_LOCAL i32 cleaned = 0;

PRIVATE void reset_stats(void) {
  started = 0;
  cleaned = 0;
}

PRIVATE void co_cleanup(anyptr stack, u64 stack_size, anyptr udata) {
  (void)udata;
  assert((i64)stack_size == STKSZ);
  assert(stack != NULL);
  xfree(stack);
  cleaned++;
}

PRIVATE void quick_start(void (*entry)(anyptr),
                         void (*cleanup)(anyptr, u64, anyptr), anyptr udata) {
  anyptr stack = xmalloc(STKSZ);
  started++;
  chiba_sco_desc d = {.stack = stack,
                      .stack_size = STKSZ,
                      .entry = entry,
                      .defer = cleanup,
                      .ctx = udata};
  chiba_sco_start(&d);
}

// sleep helper: busy-yield until ns passed
PRIVATE u64 getnow_ns(void) { return get_time_in_nanoseconds(); }
PRIVATE void sco_sleep_ns(i64 nanosecs) {
  u64 start = getnow_ns();
  while ((i64)(getnow_ns() - start) < nanosecs) {
    chiba_sco_yield();
  }
}

// ---------------- Tests ----------------

// Start and child creation order
PRIVATE THREAD_LOCAL i32 nudid = 0;
PRIVATE void co_child_entry(anyptr udata) {
  i32 udid = *(i32 *)udata;
  assert(udid == nudid);
}
PRIVATE void co_root_entry(anyptr udata) {
  assert(*(i32 *)udata == 99999999);
  assert(*(i32 *)chiba_sco_ctx() == 99999999);
  assert((i64)chiba_sco_info_all().running == 1);
  for (i32 i = 0; i < NCHILDREN; i++) {
    assert(started == 1 + i);
    assert(cleaned == 1 + i - 1);
    nudid = i;
    quick_start(co_child_entry, co_cleanup, &(i32){i});
  }
}
TEST_CASE(start_children, scheched_coroutine, "start with children", {
  DESC(start_children);
  reset_stats();
  nudid = 0;
  ASSERT_EQ(0, (i64)chiba_sco_id(), "id zero from main");
  quick_start(co_root_entry, co_cleanup, &(i32){99999999});
  ASSERT_TRUE(!chiba_sco_active(), "not active after complete");
  chiba_sco_info info = chiba_sco_info_all();
  ASSERT_EQ(0, info.detached, "detached 0");
  ASSERT_EQ(0, info.paused, "paused 0");
  ASSERT_EQ(0, info.running, "running 0");
  ASSERT_EQ(0, info.scheduled, "scheduled 0");
  ASSERT_EQ(NCHILDREN + 1, started, "all started");
  ASSERT_EQ(NCHILDREN + 1, cleaned, "all cleaned");
  return 0;
})

PRIVATE void co_sleep(anyptr u) {
  assert(*(i32 *)u == 99999999);
  sco_sleep_ns(100000000); // ~100ms busy yield
}
TEST_CASE(sleep_yielding, scheched_coroutine, "sleep via yield loop", {
  DESC(sleep_yielding);
  reset_stats();
  quick_start(co_sleep, co_cleanup, &(i32){99999999});
  return 0;
})

TEST_CASE(various_info, scheched_coroutine, "info method non-null", {
  DESC(various_info);
  reset_stats();
  ASSERT_NOT_NULL(chiba_sco_info_all().method, "method string not null");
  return 0;
})

// pause/resume
PRIVATE i64 paused_ids[NCHILDREN] = {0};
PRIVATE bool is_paused[NCHILDREN] = {0};
PRIVATE i32 npaused = 0;
PRIVATE bool all_resumed = false;
PRIVATE void co_pause_one(anyptr udata) {
  i32 index = *(i32 *)udata;
  assert(index == npaused);
  paused_ids[index] = chiba_sco_id();

  // in-order pause
  npaused++;
  is_paused[index] = true;
  chiba_sco_pause();
  is_paused[index] = false;
  npaused--;
  while (!all_resumed)
    chiba_sco_yield();

  // reverse
  sco_sleep_ns(1000000 * (NCHILDREN - index));
  npaused++;
  is_paused[index] = true;
  chiba_sco_pause();
  is_paused[index] = false;
  npaused--;
  while (!all_resumed)
    chiba_sco_yield();

  // in-order again
  npaused++;
  is_paused[index] = true;
  chiba_sco_pause();
  is_paused[index] = false;
  npaused--;
  while (!all_resumed)
    chiba_sco_yield();

  // reverse again
  sco_sleep_ns(1000000 * (NCHILDREN - index));
  npaused++;
  is_paused[index] = true;
  chiba_sco_pause();
  is_paused[index] = false;
  npaused--;
  while (!all_resumed)
    chiba_sco_yield();
}
PRIVATE void co_resume_all(anyptr u) {
  (void)u;
  // wait for all paused
  while (npaused < NCHILDREN)
    chiba_sco_yield();
  all_resumed = false;
  for (i32 i = 0; i < NCHILDREN; i++)
    chiba_sco_resume(paused_ids[i]);
  while (npaused > 0)
    chiba_sco_yield();
  all_resumed = true;

  while (npaused < NCHILDREN)
    chiba_sco_yield();
  all_resumed = false;
  for (i32 i = 0; i < NCHILDREN; i++)
    chiba_sco_resume(paused_ids[i]);
  while (npaused > 0)
    chiba_sco_yield();
  all_resumed = true;

  while (npaused < NCHILDREN)
    chiba_sco_yield();
  all_resumed = false;
  for (i32 i = NCHILDREN - 1; i >= 0; i--)
    chiba_sco_resume(paused_ids[i]);
  while (npaused > 0)
    chiba_sco_yield();
  all_resumed = true;

  while (npaused < NCHILDREN)
    chiba_sco_yield();
  all_resumed = false;
  for (i32 i = NCHILDREN - 1; i >= 0; i--)
    chiba_sco_resume(paused_ids[i]);
  while (npaused > 0)
    chiba_sco_yield();
  all_resumed = true;
}
TEST_CASE(pause_and_resume, scheched_coroutine, "pause/resume flow", {
  DESC(pause_and_resume);
  reset_stats();
  memset(paused_ids, 0, sizeof(paused_ids));
  memset(is_paused, 0, sizeof(is_paused));
  npaused = 0;
  all_resumed = false;
  for (i32 i = 0; i < NCHILDREN; i++)
    quick_start(co_pause_one, co_cleanup, &i);
  quick_start(co_resume_all, co_cleanup, 0);
  while (chiba_sco_active()) {
    chiba_sco_resume(0);
  }
  ASSERT_EQ(0, npaused, "no paused after run");
  return 0;
})

// exit ordering test
PRIVATE i32 exitvals[6] = {0};
PRIVATE i32 nexitvals = 0;
PRIVATE void co_two(anyptr u) {
  (void)u;
  sco_sleep_ns(10000000 * 2);
  exitvals[nexitvals++] = 2;
}
PRIVATE void co_three(anyptr u) {
  (void)u;
  sco_sleep_ns(10000000);
  exitvals[nexitvals++] = 3;
}
PRIVATE void co_four(anyptr u) {
  (void)u;
  exitvals[nexitvals++] = 4;
  chiba_sco_yield();
}
PRIVATE void co_one(anyptr u) {
  (void)u;
  exitvals[nexitvals++] = 1;
  quick_start(co_two, co_cleanup, 0);
  quick_start(co_three, co_cleanup, 0);
  quick_start(co_four, co_cleanup, 0);
  chiba_sco_exit();
}
TEST_CASE(exit_order, scheched_coroutine, "exit ordering", {
  DESC(exit_order);
  memset(exitvals, 0, sizeof(exitvals));
  nexitvals = 0;
  quick_start(co_one, co_cleanup, 0);
  exitvals[nexitvals++] = -1;
  while (chiba_sco_active())
    chiba_sco_resume(0);
  exitvals[nexitvals++] = -2;
  ASSERT_EQ(6, nexitvals, "nexitvals == 6");
  ASSERT_EQ(1, exitvals[0], "v0");
  ASSERT_EQ(4, exitvals[1], "v1");
  ASSERT_EQ(-1, exitvals[2], "v2");
  ASSERT_EQ(3, exitvals[3], "v3");
  ASSERT_EQ(2, exitvals[4], "v4");
  ASSERT_EQ(-2, exitvals[5], "v5");
  return 0;
})

// detach/attach across threads (mirrors sco_detach test)
#ifndef __EMSCRIPTEN__
PRIVATE i64 thpaused[NCHILDREN] = {0};
PRIVATE void co_thread_one(anyptr udata) {
  i32 index = *(i32 *)udata;
  i64 id = chiba_sco_id();
  thpaused[index] = id;
  sco_sleep_ns(1000000); // ~1ms busy-yield sleep
  chiba_sco_pause();
}
PRIVATE void *thread0_func(void *arg) {
  (void)arg;
  reset_stats();
  for (i32 i = 0; i < NCHILDREN; i++) {
    quick_start(co_thread_one, co_cleanup, &i);
  }
  // Drive runloop until all coroutines paused then detach them
  while (chiba_sco_active()) {
    chiba_sco_info info = chiba_sco_info_all();
    if (info.paused == NCHILDREN) {
      for (i32 i = 0; i < NCHILDREN; i++) {
        chiba_sco_detach(thpaused[i]);
      }
    }
    chiba_sco_resume(0);
  }
  return NULL;
}
PRIVATE void *thread1_func(void *arg) {
  (void)arg;
  reset_stats();
  // Wait for all detached
  while (chiba_sco_info_all().detached < NCHILDREN) {
    // spin
  }
  // Attach and resume each
  for (i32 i = 0; i < NCHILDREN; i++) {
    chiba_sco_attach(thpaused[i]);
    chiba_sco_resume(thpaused[i]);
  }
  while (chiba_sco_active()) {
    chiba_sco_resume(0);
  }
  return NULL;
}

TEST_CASE(detach_attach, scheched_coroutine, "detach/attach between threads", {
  DESC(detach_attach);
  pthread_t th0;
  pthread_t th1;
  ASSERT_EQ(0, pthread_create(&th0, 0, thread0_func, 0), "create th0");
  ASSERT_EQ(0, pthread_create(&th1, 0, thread1_func, 0), "create th1");
  ASSERT_EQ(0, pthread_join(th0, 0), "join th0");
  ASSERT_EQ(0, pthread_join(th1, 0), "join th1");
  ASSERT_TRUE(!chiba_sco_active(), "inactive after detach/attach cycle");
  ASSERT_EQ(0, chiba_sco_info_all().detached, "no detached remaining");
  return 0;
})
#endif

// Multi-threading test: producer + 3 consumers printing thread ids
#ifndef __EMSCRIPTEN__
PRIVATE atomic_int mt_printed = 0;
PRIVATE i64 mt_ids[NCHILDREN] = {0};
PRIVATE i32 mt_indices[NCHILDREN] = {0};
PRIVATE void co_mt_worker(anyptr u) {
  i32 idx = *(i32 *)u;
  mt_ids[idx] = chiba_sco_id();
  // Pause to be detached and migrated to another thread
  chiba_sco_pause();
  // After being attached & resumed on a consumer, print thread id and exit
  printf("[co %lld] thread %p\n", (long long)chiba_sco_id(),
         (void *)pthread_self());
  atomic_fetch_add(&mt_printed, 1);
  chiba_sco_exit();
}

PRIVATE void *mt_producer(void *arg) {
  (void)arg;
  reset_stats();
  for (i32 i = 0; i < NCHILDREN; i++) {
    mt_indices[i] = i;
    quick_start(co_mt_worker, co_cleanup, &mt_indices[i]);
  }
  // Detach all once they're paused
  while (chiba_sco_active()) {
    chiba_sco_info info = chiba_sco_info_all();
    if (info.paused == NCHILDREN) {
      for (i32 i = 0; i < NCHILDREN; i++) {
        i64 id = mt_ids[i];
        if (id) {
          chiba_sco_detach(id);
        }
      }
    }
    chiba_sco_resume(0);
  }
  return NULL;
}

PRIVATE void *mt_consumer(void *arg) {
  intptr_t cid = (intptr_t)arg; // 0..ncons-1
  const i32 ncons = 3;
  (void)cid;
  reset_stats();
  // Wait until all are detached globally
  while (chiba_sco_info_all().detached < NCHILDREN) {
    // spin
  }
  // Attach and resume a stripe of ids for this consumer
  for (i32 i = (i32)cid; i < NCHILDREN; i += ncons) {
    i64 id = mt_ids[i];
    if (id) {
      chiba_sco_attach(id);
      chiba_sco_resume(id);
    }
  }
  // Drive runloop to finish all on this thread
  while (chiba_sco_active()) {
    chiba_sco_resume(0);
  }
  return NULL;
}

TEST_CASE(
    multithread_ids, scheched_coroutine, "multi-thread print thread ids", {
      DESC(multithread_ids);
      mt_printed = 0;
      memset(mt_ids, 0, sizeof(mt_ids));
      pthread_t prod;
      pthread_t cons[3];
      ASSERT_EQ(0, pthread_create(&prod, 0, mt_producer, 0), "create producer");
      for (intptr_t i = 0; i < 3; i++) {
        ASSERT_EQ(0, pthread_create(&cons[i], 0, mt_consumer, (void *)i),
                  "create consumer");
      }
      ASSERT_EQ(0, pthread_join(prod, 0), "join producer");
      for (int i = 0; i < 3; i++) {
        ASSERT_EQ(0, pthread_join(cons[i], 0), "join consumer");
      }
      ASSERT_EQ(NCHILDREN, (i64)atomic_load(&mt_printed),
                "all coroutines printed");
      ASSERT_TRUE(!chiba_sco_active(), "inactive after multithread test");
      ASSERT_EQ(0, chiba_sco_info_all().detached, "no detached remaining");
      return 0;
    })
#endif

// scheduling order
struct order_ctx {
  u8 a[10];
  i32 i;
};
PRIVATE void co_yield1(anyptr u) {
  struct order_ctx *c = (struct order_ctx *)u;
  c->a[c->i++] = 'B';
  chiba_sco_yield();
  c->a[c->i++] = 'F';
}
PRIVATE void co_yield2(anyptr u) {
  struct order_ctx *c = (struct order_ctx *)u;
  c->a[c->i++] = 'D';
  chiba_sco_yield();
  c->a[c->i++] = 'G';
}
PRIVATE void co_yield (anyptr u) {
  struct order_ctx *c = (struct order_ctx *)u;
  c->a[c->i++] = 'A';
  quick_start(co_yield1, co_cleanup, u);
  c->a[c->i++] = 'C';
  quick_start(co_yield2, co_cleanup, u);
  c->a[c->i++] = 'E';
  chiba_sco_yield();
  c->a[c->i++] = 'H';
}
TEST_CASE(ordering, scheched_coroutine, "scheduling order", {
  DESC(ordering);
  struct order_ctx ctx = {0};
  quick_start(co_yield, co_cleanup, &ctx);
  ctx.a[ctx.i++] = '\0';
  ASSERT_TRUE(strcmp((const char *)ctx.a, "ABCDEFGH") == 0,
              "order string matches");
  return 0;
})

// Register tests
REGISTER_TEST_GROUP(scheched_coroutine) {
  // REGISTER_TEST(start_children, scheched_coroutine);
  // REGISTER_TEST(sleep_yielding, scheched_coroutine);
  // REGISTER_TEST(pause_and_resume, scheched_coroutine);
  // REGISTER_TEST(exit_order, scheched_coroutine);
  // REGISTER_TEST(ordering, scheched_coroutine);

#ifndef __EMSCRIPTEN__
  REGISTER_TEST(multithread_ids, scheched_coroutine);
  // REGISTER_TEST(detach_attach, scheched_coroutine);
#endif
  // REGISTER_TEST(various_info, scheched_coroutine);
}

ENABLE_TEST_GROUP(scheched_coroutine)
