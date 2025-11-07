#pragma once
#include "../basic_memory.h"
#include "../concurrency/array_queue.h"
#include "../concurrency/dequeue.h"
#include "../context/context.h"
#include "../utils/chiba_sem.h"
#include "fiber.h"

// Internal fiber state structure
typedef struct chiba_fiber {
  bool dead;
  bool detached;

  chiba_fiber_start_fn start;
  anyptr calling_conv;
  // join synchronization
  pthread_mutex_t join_mutex;
  pthread_cond_t join_cond;

  // Unified calling convention storage. Exact contents depend on build flags.
#if defined(CHIBA_CO_STACKJMP)
  jmp_buf buf;
#if defined(CHIBA_CO_UCONTEXT)
  struct CHIBA_co_uctx uctx;
#endif
#elif defined(CHIBA_CO_ASM)
  struct CHIBA_co_asmctx asmctx;
#elif defined(CHIBA_CO_WASM)
  emscripten_fiber_t fiber;
#endif

  // Stack info (always internally allocated via CHIBA_INTERNAL_malloc;
  // guard_size advisory only for now).
  anyptr stack_base; // base address returned by CHIBA_INTERNAL_malloc
  u64 stack_size;    // usable stack bytes
} chiba_fiber;

// Scheduler worker structure
typedef struct chiba_fiber_worker {
  pthread_t thread;
  i32 id;
  chiba_fiber_scheduler *sched;
  chiba_wsqueue *localq; // owner push/pop, others steal
  chiba_shared_ptr_param(chiba_fiber) current;
  _Atomic(bool) running;
} chiba_fiber_worker;

// Scheduler structure
typedef struct chiba_fiber_scheduler {
  i32 num_workers;
  chiba_fiber_worker *workers;
  chiba_arrayqueue
      *fallback_q; // bounded MPMC for overflow / cross-sched submissions
  _Atomic(bool) stopping;
  _Atomic(u32) live_fibers; // total fibers not fully released
  chiba_sem wake_sem;       // wake sleeping workers
} chiba_fiber_scheduler;

// Thread-local pointer to current worker for fast access
THREAD_LOCAL extern chiba_fiber_worker *chiba_tls_worker;
THREAD_LOCAL extern chiba_shared_ptr_param(chiba_fiber) chiba_tls_current_fiber;

void chiba_fiber_run_entry(chiba_shared_ptr_param(chiba_fiber) fiber);

// Scheduling primitives
void chiba_fiber_schedule_local(chiba_fiber_worker *w,
                                chiba_shared_ptr_param(chiba_fiber) f);
void chiba_fiber_schedule_external(chiba_fiber_scheduler *sched,
                                   chiba_shared_ptr_param(chiba_fiber) f);

chiba_shared_ptr_param(chiba_fiber)
    chiba_fiber_try_pop_local(chiba_fiber_worker *w);
chiba_shared_ptr_param(chiba_fiber)
    chiba_fiber_try_steal(chiba_fiber_worker *w);
chiba_shared_ptr_param(chiba_fiber)
    chiba_fallback_try_pop(chiba_fiber_scheduler *sched);
