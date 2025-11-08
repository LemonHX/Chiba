// Chiba Coroutine Context Switching Implementation
// This file provides low-level coroutine context switching functionality
// using the most efficient method available for the target platform:
// - Assembly implementations for AArch64, x86-64 (SysV/Windows), and RISC-V 64
// - Ucontext as a portable fallback
// - Emscripten fibers for WebAssembly

#pragma once

////////////////////////////////////////////////////////////////////////////////
// Platform-specific context switching implementations
////////////////////////////////////////////////////////////////////////////////

// Try assembly implementations first (fastest)
#include "context_asm_impl.h"

// Fall back to Emscripten fibers for WebAssembly
#include "context_emscripten.h"

// Fall back to ucontext for other platforms
#include "context_ucontext.h"

// Ensure we have at least one implementation
#ifndef CHIBA_CO_READY
#error                                                                         \
    "No coroutine context switching implementation available for this platform"
#endif
