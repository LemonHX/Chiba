#pragma once
#include "llco.h"

////////////////////////////////////////////////////////////////////////////////
// Architecture-specific assembly implementations
// This file dispatches to the appropriate implementation based on the target
// architecture and platform.
////////////////////////////////////////////////////////////////////////////////

#if defined(__ARM_EABI__) && !defined(LLCO_NOASM)
#include "llco_asm_arm32.h"
#elif defined(__aarch64__) && !defined(LLCO_NOASM)
#include "llco_asm_aarch64.h"
#elif defined(__riscv) && !defined(LLCO_NOASM)
#include "llco_asm_riscv.h"
#elif (defined(__i386) || defined(__i386__)) && !defined(LLCO_NOASM)
#include "llco_asm_i386.h"
#elif (defined(__x86_64__) || defined(_M_X64)) && defined(_WIN32) &&           \
    !defined(LLCO_NOASM)
#include "llco_asm_amd64_windows.h"
#elif (defined(__x86_64__) || defined(_M_X64)) && !defined(_WIN32) &&          \
    !defined(LLCO_NOASM)
#include "llco_asm_amd64_sysv.h"
#endif

// --- END ASM Code --- //
