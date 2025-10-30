// Ucontext-based coroutine context switching implementation
// Adapted from llco (https://github.com/tidwall/llco)
// Copyright (c) 2024 Joshua J Baker.

#pragma once
#include "../../basic_types.h"

#if !defined(CHIBA_CO_READY)
#define CHIBA_CO_UCONTEXT
#define CHIBA_CO_READY
#define CHIBA_CO_METHOD "ucontext"
#ifndef CHIBA_CO_STACKJMP
#define CHIBA_CO_STACKJMP
#endif

#if defined(__FreeBSD__) || defined(__APPLE__)
#ifdef __clang__
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#define _XOPEN_SOURCE
#endif
#include <ucontext.h>

PRIVATE THREAD_LOCAL ucontext_t CHIBA_co_stackjmp_ucallee;
PRIVATE THREAD_LOCAL i32 CHIBA_co_stackjmp_ucallee_gotten = 0;

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

// Ucontext always uses stackjmp with setjmp/longjmp, instead of swapcontext
// because it's much faster.
NOINLINE PRIVATE void CHIBA_co_stackjmp(anyptr stack, u64 stack_size,
                                        void (*entry)(anyptr arg))
    __attribute__((noreturn));

PRIVATE void CHIBA_co_stackjmp(anyptr stack, u64 stack_size,
                               void (*entry)(anyptr arg)) {
  if (!CHIBA_co_stackjmp_ucallee_gotten) {
    CHIBA_co_stackjmp_ucallee_gotten = 1;
    getcontext(&CHIBA_co_stackjmp_ucallee);
  }
  CHIBA_co_stackjmp_ucallee.uc_stack.ss_sp = stack;
  CHIBA_co_stackjmp_ucallee.uc_stack.ss_size = stack_size;
  CHIBA_co_adjust_ucontext_stack(&CHIBA_co_stackjmp_ucallee);
  makecontext(&CHIBA_co_stackjmp_ucallee, (void (*)(void))entry, 0);
  setcontext(&CHIBA_co_stackjmp_ucallee);
  _Exit(0); // Should never reach here
}

#endif // !CHIBA_CO_READY
