//////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////

#include "coroutine.h"

#include "./context/context.c"

// Exit function for coroutines
NOINLINE PRIVATE void CHIBA_co_exit(void) __attribute__((noreturn));
PRIVATE void CHIBA_co_exit(void) { _Exit(0); }

// Coroutine structure
struct CHIBA_co {
  struct CHIBA_co_desc desc;
#if defined(CHIBA_CO_STACKJMP)
  jmp_buf buf;
#if defined(CHIBA_CO_UCONTEXT)
  struct CHIBA_co_uctx uctx;
#endif
#elif defined(CHIBA_CO_ASM)
  struct CHIBA_co_asmctx ctx;
#elif defined(CHIBA_CO_WASM)
  emscripten_fiber_t fiber;
#endif
};

PRIVATE THREAD_LOCAL struct CHIBA_co CHIBA_co_thread = {0};
PRIVATE THREAD_LOCAL struct CHIBA_co *CHIBA_co_cur = NULL;
PRIVATE THREAD_LOCAL struct CHIBA_co_desc CHIBA_co_desc_temp;
PRIVATE THREAD_LOCAL volatile bool CHIBA_co_defer_needed = false;
PRIVATE THREAD_LOCAL volatile struct CHIBA_co_desc CHIBA_co_defer_desc;
PRIVATE THREAD_LOCAL volatile bool CHIBA_co_defer_active = false;

// Guard macro to prevent certain operations during defer
#define CHIBA_co_defer_guard()                                                 \
  {                                                                            \
    if (CHIBA_co_defer_active) {                                               \
      fprintf(stderr, "%s not available during defer\n", __func__);            \
      abort();                                                                 \
    }                                                                          \
  }

// Execute pending defer handler
PRIVATE void CHIBA_co_defer_last(void) {
  if (CHIBA_co_defer_needed) {
    if (CHIBA_co_defer_desc.defer) {
      CHIBA_co_defer_active = true;
      CHIBA_co_defer_desc.defer(CHIBA_co_defer_desc.stack,
                                CHIBA_co_defer_desc.stack_size,
                                CHIBA_co_defer_desc.ctx);
      CHIBA_co_defer_active = false;
    }
    CHIBA_co_defer_needed = false;
  }
}

// Coroutine entry wrapper
NOINLINE
PRIVATE void CHIBA_co_entry_wrap(anyptr arg) {
  CHIBA_co_defer_last();
#if defined(CHIBA_CO_WASM)
  CHIBA_co_cur = arg;
  CHIBA_co_cur->desc = CHIBA_co_desc_temp;
#else
  (void)arg;
  struct CHIBA_co self = {.desc = CHIBA_co_desc_temp};
  CHIBA_co_cur = &self;
#endif
  CHIBA_co_cur->desc.entry(CHIBA_co_cur->desc.ctx);
}

// Coroutine entry point (externally visible for asm context creation)
NOINLINE void CHIBA_co_entry(anyptr arg) {
  CHIBA_co_entry_wrap(arg);
  CHIBA_co_exit();
}

// Low-level context switch
NOINLINE
PRIVATE void chiba_co_switch1(struct CHIBA_co *from, struct CHIBA_co *to,
                              anyptr stack, u64 stack_size) {
#if defined(CHIBA_CO_STACKJMP)
  if (to) {
    if (!_setjmp(from->buf)) {
      _longjmp(to->buf, 1);
    }
  } else {
    if (!_setjmp(from->buf)) {
      CHIBA_co_stackjmp(&from->uctx, stack, stack_size, CHIBA_co_entry);
    }
  }
#elif defined(CHIBA_CO_ASM)
  if (to) {
    _CHIBA_co_asm_switch(&from->ctx, &to->ctx);
  } else {
    struct CHIBA_co_asmctx ctx = {0};
    CHIBA_co_asmctx_make(&ctx, stack, stack_size, 0);
    _CHIBA_co_asm_switch(&from->ctx, &ctx);
  }
#elif defined(CHIBA_CO_WASM)
  if (to) {
    emscripten_fiber_swap(&from->fiber, &to->fiber);
  } else {
    if (from == &CHIBA_co_thread) {
      emscripten_fiber_init_from_current_context(
          &from->fiber, CHIBA_co_main_stack, CHIBA_CO_ASYNCIFY_STACK_SIZE);
    }
    stack_size -= CHIBA_CO_ASYNCIFY_STACK_SIZE;
    i8 *astack = ((i8 *)stack) + stack_size;
    u64 astack_size = CHIBA_CO_ASYNCIFY_STACK_SIZE - sizeof(struct CHIBA_co);
    struct CHIBA_co *self = (anyptr)(astack + astack_size);
    memset(self, 0, sizeof(struct CHIBA_co));
    emscripten_fiber_init(&self->fiber, CHIBA_co_entry, self, stack, stack_size,
                          astack, astack_size);
    emscripten_fiber_swap(&from->fiber, &self->fiber);
  }
#endif
}

// High-level context switch
PRIVATE void chiba_co_switch0(struct CHIBA_co_desc *desc, struct CHIBA_co *co,
                              bool final) {
  struct CHIBA_co *from = CHIBA_co_cur ? CHIBA_co_cur : &CHIBA_co_thread;
  struct CHIBA_co *to = desc ? NULL : co ? co : &CHIBA_co_thread;
  if (from != to) {
    if (final) {
      CHIBA_co_defer_needed = true;
      memcpy((void *)&CHIBA_co_defer_desc, &from->desc,
             sizeof(struct CHIBA_co_desc));
    }
    if (desc) {
      CHIBA_co_desc_temp = *desc;
      chiba_co_switch1(from, NULL, desc->stack, desc->stack_size);
    } else {
      CHIBA_co_cur = to;
      chiba_co_switch1(from, to, NULL, 0);
    }
    CHIBA_co_defer_last();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Public API Implementation
////////////////////////////////////////////////////////////////////////////////

// Start a new coroutine
PUBLIC void chiba_co_start(struct CHIBA_co_desc *desc, bool final) {
  if (!desc || desc->stack_size < CHIBA_CO_MINSTACKSIZE) {
    fprintf(stderr, "chiba_co_start: stack too small (minimum %d bytes)\n",
            CHIBA_CO_MINSTACKSIZE);
    abort();
  }
  CHIBA_co_defer_guard();
  chiba_co_switch0(desc, NULL, final);
}

// Switch to another coroutine
PUBLIC void chiba_co_switch(struct CHIBA_co *co, bool final) {
#if defined(CHIBA_CO_ASM)
  // Fast track context switch. Saves a few nanoseconds by checking the
  // exception condition first.
  if (!CHIBA_co_defer_active && CHIBA_co_cur && co && CHIBA_co_cur != co &&
      !final) {
    struct CHIBA_co *from = CHIBA_co_cur;
    CHIBA_co_cur = co;
    _CHIBA_co_asm_switch(&from->ctx, &co->ctx);
    CHIBA_co_defer_last();
    return;
  }
#endif
  CHIBA_co_defer_guard();
  chiba_co_switch0(NULL, co, final);
}

// Return the current coroutine or NULL if not currently running in a coroutine
PUBLIC struct CHIBA_co *chiba_co_current(void) {
  CHIBA_co_defer_guard();
  return CHIBA_co_cur == &CHIBA_co_thread ? NULL : CHIBA_co_cur;
}

// Returns a string that indicates which coroutine method is being used
PUBLIC cstr chiba_co_method(anyptr caps) {
  (void)caps;
  return CHIBA_CO_METHOD;
}
