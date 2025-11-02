// Ucontext-based coroutine context switching implementation
// Adapted from llco (https://github.com/tidwall/llco)
// Copyright (c) 2024 Joshua J Baker.

#pragma once
#include "../../basic_types.h"

#if !defined(CHIBA_CO_READY)
#define CHIBA_CO_UCONTEXT
#define CHIBA_CO_READY
#ifndef CHIBA_CO_STACKJMP
#define CHIBA_CO_STACKJMP
#endif
#define CHIBA_CO_METHOD "ucontext, stackjmp"

#if defined(__FreeBSD__) || defined(__APPLE__)
#ifdef __clang__
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#define _XOPEN_SOURCE
#endif
#include <ucontext.h>

// Ucontext context structure - stores per-coroutine state
struct CHIBA_co_uctx {
  ucontext_t ucallee;
  i32 initialized;
};

#if defined(__APPLE__) && defined(__aarch64__) &&                              \
    !defined(CHIBA_CO_NOSTACKADJUST)
// Here we ensure that the initial context switch will *not* page the
// entire stack into process memory before executing the entry point
// function. This is a behavior that can be observed on Mac OS with
// Apple Silicon. This "trick" can be optionally removed at the expense
// of slower initial jumping into large stacks.
enum CHIBA_co_stack_grows { DOWNWARDS, UPWARDS };

PRIVATE enum CHIBA_co_stack_grows CHIBA_co_stack_grows0(i32 *addr0) {
  i32 addr1;
  return addr0 < &addr1 ? UPWARDS : DOWNWARDS;
}

PRIVATE enum CHIBA_co_stack_grows CHIBA_co_stack_grows(void) {
  i32 addr0;
  return CHIBA_co_stack_grows0(&addr0);
}

PRIVATE void CHIBA_co_adjust_ucontext_stack(ucontext_t *ucp) {
  if (CHIBA_co_stack_grows() == UPWARDS) {
    ucp->uc_stack.ss_sp = (i8 *)ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size;
    ucp->uc_stack.ss_size = 0;
  }
}
#else
#define CHIBA_co_adjust_ucontext_stack(ucp)
#endif

// Initialize a ucontext context structure
PRIVATE void CHIBA_co_uctx_make(struct CHIBA_co_uctx *ctx, anyptr stack_base,
                                u64 stack_size, void (*entry)(anyptr arg)) {
  if (!ctx->initialized) {
    ctx->initialized = 1;
    getcontext(&ctx->ucallee);
  }
  ctx->ucallee.uc_stack.ss_sp = stack_base;
  ctx->ucallee.uc_stack.ss_size = stack_size;
  CHIBA_co_adjust_ucontext_stack(&ctx->ucallee);
  makecontext(&ctx->ucallee, (void (*)(void))entry, 0);
}

// Ucontext always uses stackjmp with setjmp/longjmp, instead of swapcontext
// because it's much faster.
NOINLINE PRIVATE void CHIBA_co_stackjmp(struct CHIBA_co_uctx *ctx, anyptr stack,
                                        u64 stack_size,
                                        void (*entry)(anyptr arg))
    __attribute__((noreturn));

PRIVATE void CHIBA_co_stackjmp(struct CHIBA_co_uctx *ctx, anyptr stack,
                               u64 stack_size, void (*entry)(anyptr arg)) {
  CHIBA_co_uctx_make(ctx, stack, stack_size, entry);
  setcontext(&ctx->ucallee);
  _Exit(0); // Should never reach here
}

#endif // !CHIBA_CO_READY
