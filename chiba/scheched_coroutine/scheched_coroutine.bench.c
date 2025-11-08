#include "scheched_coroutine.h"
#include "../../include/utils/chiba_utils_timer.h"
#include "../basic_memory.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// -----------------------------------------------
// Benchmark configuration
// -----------------------------------------------
#define STKSZ 32768
#define SWITCH_TARGET 1000000LL // 总切换次数

typedef struct {
  double elapsed_us;    // 总耗时 (microseconds)
  double throughput_ms; // 每毫秒切换次数 (switches / ms)
  double per_switch_us; // 单次切换耗时 (microseconds)
} BenchResult;

static inline u64 now_ns(void) { return get_time_in_nanoseconds(); }

// -----------------------------------------------
// Coroutine ping-pong (single-thread, condvar-like)
// -----------------------------------------------
static atomic_long co_switch_count = 0;
static atomic_int co_stop = 0;
static volatile int co_turn = 0; // 0 or 1 (used only for legacy path)
static atomic_llong co_ids[2];

static void co_free_stack(anyptr s, u64 n, anyptr u) {
  (void)n;
  (void)u;
  if (s)
    CHIBA_INTERNAL_free(s);
}

static void co_pingpong(anyptr u) {
  int me = (int)(uintptr_t)u; // 0 or 1
  // publish my id for driver
  atomic_store(&co_ids[me], (long long)chiba_sco_id());
  for (;;) {
    if (atomic_load_explicit(&co_stop, memory_order_relaxed)) {
      chiba_sco_exit();
    }
    atomic_fetch_add_explicit(&co_switch_count, 1, memory_order_relaxed);
    // park until resumed by driver
    chiba_sco_pause();
  }
}

static BenchResult bench_coroutine_pingpong(void) {
  atomic_store(&co_switch_count, 0);
  atomic_store(&co_stop, 0);
  atomic_store(&co_ids[0], 0);
  atomic_store(&co_ids[1], 0);

  chiba_sco_desc d0 = {0}, d1 = {0};
  d0.stack = CHIBA_INTERNAL_malloc(STKSZ);
  d0.stack_size = STKSZ;
  d0.entry = co_pingpong;
  d0.defer = co_free_stack;
  d0.ctx = (anyptr)(uintptr_t)0;
  chiba_sco_start(&d0);

  d1.stack = CHIBA_INTERNAL_malloc(STKSZ);
  d1.stack_size = STKSZ;
  d1.entry = co_pingpong;
  d1.defer = co_free_stack;
  d1.ctx = (anyptr)(uintptr_t)1;
  chiba_sco_start(&d1);

  // let both coroutines run once to publish their IDs and pause
  while ((atomic_load(&co_ids[0]) == 0 || atomic_load(&co_ids[1]) == 0) &&
         chiba_sco_active()) {
    chiba_sco_resume(0);
  }

  long long id0 = atomic_load(&co_ids[0]);
  long long id1 = atomic_load(&co_ids[1]);

  // both should be paused now; do controlled ping-pong
  int turn = 0;
  u64 start_ns = now_ns();
  for (long long i = 0; i < SWITCH_TARGET; ++i) {
    chiba_sco_resume(turn == 0 ? id0 : id1);
    turn ^= 1;
  }
  atomic_store(&co_stop, 1);
  // wake both to let them exit
  chiba_sco_resume(id0);
  chiba_sco_resume(id1);
  // drain runloop
  while (chiba_sco_active()) {
    chiba_sco_resume(0);
  }
  u64 end_ns = now_ns();

  double elapsed_us = (double)(end_ns - start_ns) / 1000.0;
  BenchResult r;
  r.elapsed_us = elapsed_us;
  r.throughput_ms = (SWITCH_TARGET) / (elapsed_us / 1000.0); // switches/ms
  r.per_switch_us = elapsed_us / SWITCH_TARGET;
  return r;
}

// -----------------------------------------------
// pthread ping-pong (2 threads, condvar)
// -----------------------------------------------
typedef struct {
  pthread_mutex_t m;
  pthread_cond_t c;
  int turn; // 0 or 1
  atomic_long count;
  atomic_int stop;
} PingPong;

static void *pp_worker(void *arg) {
  PingPong *pp = (PingPong *)arg;
  pthread_mutex_lock(&pp->m);
  for (;;) {
    while (pp->turn != 1 &&
           !atomic_load_explicit(&pp->stop, memory_order_relaxed)) {
      pthread_cond_wait(&pp->c, &pp->m);
    }
    if (atomic_load_explicit(&pp->stop, memory_order_relaxed)) {
      pthread_mutex_unlock(&pp->m);
      return NULL;
    }
    long v = atomic_fetch_add_explicit(&pp->count, 1, memory_order_relaxed);
    if (v >= SWITCH_TARGET) {
      atomic_store(&pp->stop, 1);
      pthread_cond_broadcast(&pp->c);
      pthread_mutex_unlock(&pp->m);
      return NULL;
    }
    pp->turn = 0;
    pthread_cond_signal(&pp->c);
  }
}

static BenchResult bench_pthread_pingpong(void) {
  PingPong pp;
  pthread_mutex_init(&pp.m, NULL);
  pthread_cond_init(&pp.c, NULL);
  pp.turn = 0;
  atomic_store(&pp.stop, 0);
  atomic_store(&pp.count, 0);

  pthread_t th;
  pthread_create(&th, NULL, pp_worker, &pp);

  u64 start_ns = now_ns();

  pthread_mutex_lock(&pp.m);
  for (;;) {
    while (pp.turn != 0 &&
           !atomic_load_explicit(&pp.stop, memory_order_relaxed)) {
      pthread_cond_wait(&pp.c, &pp.m);
    }
    if (atomic_load_explicit(&pp.stop, memory_order_relaxed)) {
      pthread_mutex_unlock(&pp.m);
      break;
    }
    long v = atomic_fetch_add_explicit(&pp.count, 1, memory_order_relaxed);
    if (v >= SWITCH_TARGET) {
      atomic_store(&pp.stop, 1);
      pthread_cond_broadcast(&pp.c);
      pthread_mutex_unlock(&pp.m);
      break;
    }
    pp.turn = 1;
    pthread_cond_signal(&pp.c);
  }

  pthread_join(th, NULL);

  u64 end_ns = now_ns();

  pthread_mutex_destroy(&pp.m);
  pthread_cond_destroy(&pp.c);

  double elapsed_us = (double)(end_ns - start_ns) / 1000.0;
  BenchResult r;
  r.elapsed_us = elapsed_us;
  r.throughput_ms = (SWITCH_TARGET) / (elapsed_us / 1000.0); // switches/ms
  r.per_switch_us = elapsed_us / SWITCH_TARGET;
  return r;
}

// -----------------------------------------------
// Main
// -----------------------------------------------
int main(void) {
  printf("========================================\n");
  printf("  Coroutine vs pthread Ping-Pong Benchmark\n");
  printf("========================================\n");
  printf("  Target switches: %lld\n", (long long)SWITCH_TARGET);
  printf("\n");

  // Benchmark 1: Coroutine ping-pong
  printf("========================================\n");
  printf("Benchmark 1: Coroutine ping-pong (single-thread)\n");
  printf("========================================\n");
  BenchResult co = bench_coroutine_pingpong();
  printf("  Time: %.2f us\n", co.elapsed_us);
  printf("  Throughput: %.2f switches/ms\n", co.throughput_ms);
  printf("  Avg switch: %.4f us\n\n", co.per_switch_us);

  // Benchmark 2: pthread ping-pong
  printf("========================================\n");
  printf("Benchmark 2: pthread ping-pong (condvar)\n");
  printf("========================================\n");
  BenchResult pt = bench_pthread_pingpong();
  printf("  Time: %.2f us\n", pt.elapsed_us);
  printf("  Throughput: %.2f switches/ms\n", pt.throughput_ms);
  printf("  Avg switch: %.4f us\n\n", pt.per_switch_us);

  // Summary
  printf("========================================\n");
  printf("Summary\n");
  printf("========================================\n");
  printf("coroutine ping-pong:  %.2f us (%.2f switches/ms, %.4f us/switch)\n",
         co.elapsed_us, co.throughput_ms, co.per_switch_us);
  printf("pthread   ping-pong:  %.2f us (%.2f switches/ms, %.4f us/switch)\n",
         pt.elapsed_us, pt.throughput_ms, pt.per_switch_us);

  double factor = pt.elapsed_us / co.elapsed_us;
  printf("\nSpeed factor (elapsed, >1 => pthread slower): %.2fx\n", factor);
  printf("========================================\n");
  return 0;
}
