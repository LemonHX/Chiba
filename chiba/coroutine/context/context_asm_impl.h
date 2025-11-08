// Assembly context switching implementation dispatcher
// Conditionally includes the appropriate architecture-specific implementation

#pragma once
#include "../../basic_types.h"

// Check for CosmopolillanC which has issues with asm code
#if defined(__COSMOCC__) && !defined(CHIBA_CO_NOASM)
#define CHIBA_CO_NOASM
#endif

// Include architecture-specific implementations
#if defined(__aarch64__) && !defined(CHIBA_CO_NOASM)
#include "context_asm_aarch64_sysv.h"
#elif (defined(__x86_64__) || defined(_M_X64)) && !defined(CHIBA_CO_NOASM)
#ifdef _WIN32
#include "context_asm_amd64_windows.h"
#else
#include "context_asm_amd64_sysv.h"
#endif
#elif defined(__riscv) && !defined(CHIBA_CO_NOASM) && (__riscv_xlen == 64)
#include "context_asm_riscv64_sysv.h"
#endif // CHIBA_CO_ASM
