//////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////

#include "coroutine.h"

#include "./context/context.c"

// Exit function for coroutines
NOINLINE PRIVATE void chiba_co_exit(void) __attribute__((noreturn));
PRIVATE void chiba_co_exit(void) { _Exit(0); }

// Coroutine structure
typedef struct chiba_co {
  struct chiba_co_desc desc;
#if defined(CHIBA_CO_STACKJMP)
  jmp_buf buf;
#if defined(CHIBA_CO_UCONTEXT)
  struct chiba_co_uctx uctx;
#endif
#elif defined(CHIBA_CO_ASM)
  struct chiba_co_asmctx ctx;
#elif defined(CHIBA_CO_WASM)
  emscripten_fiber_t fiber;
#endif
} chiba_co;

PRIVATE THREAD_LOCAL struct chiba_co chiba_co_thread = {0};
PRIVATE THREAD_LOCAL struct chiba_co *chiba_co_cur = NULL;
PRIVATE THREAD_LOCAL struct chiba_co_desc chiba_co_desc_temp;
PRIVATE THREAD_LOCAL volatile bool chiba_co_defer_needed = false;
PRIVATE THREAD_LOCAL volatile struct chiba_co_desc chiba_co_defer_desc;
PRIVATE THREAD_LOCAL volatile bool chiba_co_defer_active = false;

// Guard macro to prevent certain operations during defer
#define chiba_co_defer_guard()                                                 \
  {                                                                            \
    if (chiba_co_defer_active) {                                               \
      fprintf(stderr, "%s not available during defer\n", __func__);            \
      abort();                                                                 \
    }                                                                          \
  }

// Execute pending defer handler
PRIVATE void chiba_co_defer_last(void) {
  if (chiba_co_defer_needed) {
    if (chiba_co_defer_desc.defer) {
      chiba_co_defer_active = true;
      chiba_co_defer_desc.defer(chiba_co_defer_desc.stack,
                                chiba_co_defer_desc.stack_size,
                                chiba_co_defer_desc.ctx);
      chiba_co_defer_active = false;
    }
    chiba_co_defer_needed = false;
  }
}

// Coroutine entry wrapper
NOINLINE
PRIVATE void chiba_co_entry_wrap(anyptr arg) {
  chiba_co_defer_last();
#if defined(CHIBA_CO_WASM)
  chiba_co_cur = arg;
  chiba_co_cur->desc = chiba_co_desc_temp;
#else
  (void)arg;
  struct chiba_co self = {.desc = chiba_co_desc_temp};
  chiba_co_cur = &self;
#endif
  chiba_co_cur->desc.entry(chiba_co_cur->desc.ctx);
}

// Coroutine entry point (externally visible for asm context creation)
NOINLINE void chiba_co_entry(anyptr arg) {
  chiba_co_entry_wrap(arg);
  chiba_co_exit();
}

// Low-level context switch
NOINLINE
PRIVATE void chiba_co_switch1(struct chiba_co *from, struct chiba_co *to,
                              anyptr stack, u64 stack_size) {
#if defined(CHIBA_CO_STACKJMP)
  if (to) {
    if (!_setjmp(from->buf)) {
      _longjmp(to->buf, 1);
    }
  } else {
    if (!_setjmp(from->buf)) {
      chiba_co_stackjmp(&from->uctx, stack, stack_size, chiba_co_entry);
    }
  }
#elif defined(CHIBA_CO_ASM)
  if (to) {
    _chiba_co_asm_switch(&from->ctx, &to->ctx);
  } else {
    struct chiba_co_asmctx ctx = {0};
    chiba_co_asmctx_make(&ctx, stack, stack_size, 0);
    _chiba_co_asm_switch(&from->ctx, &ctx);
  }
#elif defined(CHIBA_CO_WASM)
  if (to) {
    emscripten_fiber_swap(&from->fiber, &to->fiber);
  } else {
    if (from == &chiba_co_thread) {
      emscripten_fiber_init_from_current_context(
          &from->fiber, chiba_co_main_stack, CHIBA_CO_ASYNCIFY_STACK_SIZE);
    }
    stack_size -= CHIBA_CO_ASYNCIFY_STACK_SIZE;
    i8 *astack = ((i8 *)stack) + stack_size;
    u64 astack_size = CHIBA_CO_ASYNCIFY_STACK_SIZE - sizeof(struct chiba_co);
    struct chiba_co *self = (anyptr)(astack + astack_size);
    memset(self, 0, sizeof(struct chiba_co));
    emscripten_fiber_init(&self->fiber, chiba_co_entry, self, stack, stack_size,
                          astack, astack_size);
    emscripten_fiber_swap(&from->fiber, &self->fiber);
  }
#endif
}

// High-level context switch
PRIVATE void chiba_co_switch0(struct chiba_co_desc *desc, struct chiba_co *co,
                              bool final) {
  struct chiba_co *from = chiba_co_cur ? chiba_co_cur : &chiba_co_thread;
  struct chiba_co *to = desc ? NULL : co ? co : &chiba_co_thread;
  if (from != to) {
    if (final) {
      chiba_co_defer_needed = true;
      memcpy((void *)&chiba_co_defer_desc, &from->desc,
             sizeof(struct chiba_co_desc));
    }
    if (desc) {
      chiba_co_desc_temp = *desc;
      chiba_co_switch1(from, NULL, desc->stack, desc->stack_size);
    } else {
      chiba_co_cur = to;
      chiba_co_switch1(from, to, NULL, 0);
    }
    chiba_co_defer_last();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Public API Implementation
////////////////////////////////////////////////////////////////////////////////

// Start a new coroutine
PUBLIC void chiba_co_start(struct chiba_co_desc *desc, bool final) {
  if (!desc || desc->stack_size < CHIBA_CO_MINSTACKSIZE) {
    fprintf(stderr, "chiba_co_start: stack too small (minimum %d bytes)\n",
            CHIBA_CO_MINSTACKSIZE);
    abort();
  }
  chiba_co_defer_guard();
  chiba_co_switch0(desc, NULL, final);
}

// Switch to another coroutine
PUBLIC void chiba_co_switch(struct chiba_co *co, bool final) {
#if defined(CHIBA_CO_ASM)
  // Fast track context switch. Saves a few nanoseconds by checking the
  // exception condition first.
  if (!chiba_co_defer_active && chiba_co_cur && co && chiba_co_cur != co &&
      !final) {
    struct chiba_co *from = chiba_co_cur;
    chiba_co_cur = co;
    _chiba_co_asm_switch(&from->ctx, &co->ctx);
    chiba_co_defer_last();
    return;
  }
#endif
  chiba_co_defer_guard();
  chiba_co_switch0(NULL, co, final);
}

// Return the current coroutine or NULL if not currently running in a coroutine
PUBLIC struct chiba_co *chiba_co_current(void) {
  chiba_co_defer_guard();
  return chiba_co_cur == &chiba_co_thread ? NULL : chiba_co_cur;
}

// Returns a string that indicates which coroutine method is being used
PUBLIC cstr chiba_co_method(anyptr caps) {
  (void)caps;
  return CHIBA_CO_METHOD;
}
