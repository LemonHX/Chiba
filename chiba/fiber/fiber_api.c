#include "../arc/shared_memory.h"
#include "fiber_internal.h"

// TLS definitions
THREAD_LOCAL chiba_fiber_worker *chiba_tls_worker = NULL;
THREAD_LOCAL chiba_shared_ptr chiba_tls_current_fiber = {.control = NULL};

// Internal user context carried by a fiber
typedef struct chiba_fiber_user_ctx {
  anyptr arg;
  anyptr ret;
} chiba_fiber_user_ctx;

// Init args used for ARC init callback
typedef struct chiba_fiber_init_args {
  chiba_fiber_start_fn start;
  anyptr arg;
  bool detached;
  u64 stack_size;
} chiba_fiber_init_args;

PRIVATE void _chiba_fiber_init(anyptr self, anyptr args_any) {
  chiba_fiber *f = (chiba_fiber *)self;
  chiba_fiber_init_args *args = (chiba_fiber_init_args *)args_any;

  f->dead = false;
  f->detached = args ? args->detached : false;
  f->start = args ? args->start : NULL;

  // allocate user ctx to carry arg/ret
  chiba_fiber_user_ctx *uctx =
      (chiba_fiber_user_ctx *)CHIBA_INTERNAL_malloc(sizeof(*uctx));
  if (uctx) {
    uctx->arg = args ? args->arg : NULL;
    uctx->ret = NULL;
  }
  f->calling_conv = (anyptr)uctx;

  pthread_mutex_init(&f->join_mutex, NULL);
  pthread_cond_init(&f->join_cond, NULL);

  // allocate stack (malloc-based)
  u64 stack_size = args && args->stack_size ? args->stack_size
                                            : CHIBA_FIBER_DEFAULT_STACK_SIZE;
  if (stack_size < CHIBA_CO_MINSTACKSIZE)
    stack_size = CHIBA_CO_MINSTACKSIZE;
  f->stack_base = CHIBA_INTERNAL_malloc((size_t)stack_size);
  f->stack_size = f->stack_base ? stack_size : 0;

  // Context setup will be done by scheduler when scheduling first time.
}

PRIVATE void _chiba_fiber_drop(anyptr self) {
  chiba_fiber *f = (chiba_fiber *)self;

  // free user ctx
  if (f->calling_conv) {
    CHIBA_INTERNAL_free(f->calling_conv);
    f->calling_conv = NULL;
  }

  // free stack
  if (f->stack_base) {
    CHIBA_INTERNAL_free(f->stack_base);
    f->stack_base = NULL;
    f->stack_size = 0;
  }

  pthread_cond_destroy(&f->join_cond);
  pthread_mutex_destroy(&f->join_mutex);
}

// Public API
PUBLIC chiba_shared_ptr chiba_fiber_create(chiba_fiber_scheduler *sched,
                                           chiba_fiber_start_fn fn,
                                           anyptr arg) {
  (void)sched; // scheduling happens separately
  chiba_fiber_init_args args = {
      .start = fn,
      .arg = arg,
      .detached = false,
      .stack_size = CHIBA_FIBER_DEFAULT_STACK_SIZE,
  };
  return chiba_shared_new(sizeof(chiba_fiber), &args, _chiba_fiber_init,
                          _chiba_fiber_drop);
}

PUBLIC chiba_shared_ptr chiba_fiber_spawn(chiba_fiber_scheduler *sched,
                                          chiba_fiber_start_fn fn, anyptr arg) {
  (void)sched;
  chiba_fiber_init_args args = {
      .start = fn,
      .arg = arg,
      .detached = true,
      .stack_size = CHIBA_FIBER_DEFAULT_STACK_SIZE,
  };
  return chiba_shared_new(sizeof(chiba_fiber), &args, _chiba_fiber_init,
                          _chiba_fiber_drop);
}

PUBLIC int chiba_fiber_detach(chiba_shared_ptr fiber) {
  chiba_fiber *f = (chiba_fiber *)chiba_shared_get(&fiber);
  if (!f)
    return -1;
  pthread_mutex_lock(&f->join_mutex);
  f->detached = true;
  pthread_mutex_unlock(&f->join_mutex);
  return 0;
}

PUBLIC int chiba_fiber_join(chiba_shared_ptr fiber, anyptr *ret_ptr) {
  chiba_fiber *f = (chiba_fiber *)chiba_shared_get(&fiber);
  if (!f)
    return -1;

  pthread_mutex_lock(&f->join_mutex);
  if (f->detached) {
    pthread_mutex_unlock(&f->join_mutex);
    return -1; // cannot join detached
  }

  while (!f->dead) {
    pthread_cond_wait(&f->join_cond, &f->join_mutex);
  }

  if (ret_ptr) {
    chiba_fiber_user_ctx *uctx = (chiba_fiber_user_ctx *)f->calling_conv;
    *ret_ptr = uctx ? uctx->ret : NULL;
  }

  pthread_mutex_unlock(&f->join_mutex);
  return 0;
}

PUBLIC void chiba_fiber_exit(anyptr ret_value) {
  // To be called from inside a running fiber context. Without scheduler wiring,
  // this acts as a placeholder that marks dead and signals joiners.
  chiba_shared_ptr cur = chiba_tls_current_fiber;
  chiba_fiber *f = (chiba_fiber *)chiba_shared_get(&cur);
  if (!f) {
    CHIBA_PANIC("chiba_fiber_exit called outside of fiber");
  }

  pthread_mutex_lock(&f->join_mutex);
  chiba_fiber_user_ctx *uctx = (chiba_fiber_user_ctx *)f->calling_conv;
  if (uctx)
    uctx->ret = ret_value;
  f->dead = true;
  pthread_mutex_unlock(&f->join_mutex);
  pthread_cond_broadcast(&f->join_cond);

  // In a fully wired scheduler, we'd context-switch back.
  // For now, terminate the thread to avoid undefined continuation.
  _Exit(0);
}

PUBLIC void chiba_fiber_yield(void) {
  // Will be implemented when scheduler is wired; placeholder for now.
}

PUBLIC chiba_shared_ptr chiba_fiber_current(void) {
  return chiba_shared_clone(&chiba_tls_current_fiber);
}
