// Chiba Coroutine Context Switching Implementation
// This file provides low-level coroutine context switching functionality
// using the most efficient method available for the target platform:
// - Assembly implementations for AArch64, x86-64 (SysV/Windows), and RISC-V 64
// - Ucontext as a portable fallback
// - Emscripten fibers for WebAssembly

#pragma once
#include "../../basic_types.h"

////////////////////////////////////////////////////////////////////////////////
// Platform-specific context switching implementations
////////////////////////////////////////////////////////////////////////////////

// Try assembly implementations first (fastest)
#include "context_asm_impl.h"

// ASM with stackjmp activated
#if defined(CHIBA_CO_READY) && defined(CHIBA_CO_STACKJMP)
NOINLINE PRIVATE void CHIBA_co_stackjmp(anyptr stack, u64 stack_size,
                                        void (*entry)(anyptr arg))
    __attribute__((noreturn));

PRIVATE void CHIBA_co_stackjmp(anyptr stack, u64 stack_size,
                               void (*entry)(anyptr arg)) {
  struct CHIBA_co_asmctx ctx = {0};
  CHIBA_co_asmctx_make(&ctx, stack, stack_size, 0);
  struct CHIBA_co_asmctx ctx0 = {0};
  _CHIBA_co_asm_switch(&ctx0, &ctx);
  _Exit(0);
}
#endif

// Fall back to Emscripten fibers for WebAssembly
#include "context_emscripten.h"

// Fall back to ucontext for other platforms
#include "context_ucontext.h"

// setjmp/longjmp support for stackjmp
#if defined(CHIBA_CO_STACKJMP)
#include <setjmp.h>
#ifdef _WIN32
// For reasons outside of my understanding, Windows does not allow for jumping
// between stacks using the setjmp/longjmp mechanism.
#error Windows stackjmp not supported
#endif
#endif

// Ensure we have at least one implementation
#ifndef CHIBA_CO_READY
#error                                                                         \
    "No coroutine context switching implementation available for this platform"
#endif
