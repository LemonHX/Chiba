// ARM 64-bit (AArch64) System V ABI context switching implementation
// Adapted from llco (https://github.com/tidwall/llco)
// Copyright (c) 2024 Joshua J Baker.

#pragma once
#include "../basic_types.h"

#if defined(__aarch64__) && !defined(CHIBA_CO_NOASM) && !defined(__WIN32)
#define CHIBA_CO_ASM
#define CHIBA_CO_READY
#define CHIBA_CO_METHOD "asm,aarch64"

struct CHIBA_co_asmctx {
  anyptr x[12]; /* x19-x30 */
  anyptr sp;
  anyptr lr;
  anyptr d[8]; /* d8-d15 */
};

void _CHIBA_co_asm_entry(void);
i32 _CHIBA_co_asm_switch(struct CHIBA_co_asmctx *from,
                         struct CHIBA_co_asmctx *to);

__asm__(".text\n"
#ifdef __APPLE__
        ".globl __CHIBA_co_asm_switch\n"
        "__CHIBA_co_asm_switch:\n"
#else
        ".globl _CHIBA_co_asm_switch\n"
        ".type _CHIBA_co_asm_switch #function\n"
        ".hidden _CHIBA_co_asm_switch\n"
        "_CHIBA_co_asm_switch:\n"
#endif

        "  mov x10, sp\n"
        "  mov x11, x30\n"
        "  stp x19, x20, [x0, #(0*16)]\n"
        "  stp x21, x22, [x0, #(1*16)]\n"
        "  stp d8, d9, [x0, #(7*16)]\n"
        "  stp x23, x24, [x0, #(2*16)]\n"
        "  stp d10, d11, [x0, #(8*16)]\n"
        "  stp x25, x26, [x0, #(3*16)]\n"
        "  stp d12, d13, [x0, #(9*16)]\n"
        "  stp x27, x28, [x0, #(4*16)]\n"
        "  stp d14, d15, [x0, #(10*16)]\n"
        "  stp x29, x30, [x0, #(5*16)]\n"
        "  stp x10, x11, [x0, #(6*16)]\n"
        "  ldp x19, x20, [x1, #(0*16)]\n"
        "  ldp x21, x22, [x1, #(1*16)]\n"
        "  ldp d8, d9, [x1, #(7*16)]\n"
        "  ldp x23, x24, [x1, #(2*16)]\n"
        "  ldp d10, d11, [x1, #(8*16)]\n"
        "  ldp x25, x26, [x1, #(3*16)]\n"
        "  ldp d12, d13, [x1, #(9*16)]\n"
        "  ldp x27, x28, [x1, #(4*16)]\n"
        "  ldp d14, d15, [x1, #(10*16)]\n"
        "  ldp x29, x30, [x1, #(5*16)]\n"
        "  ldp x10, x11, [x1, #(6*16)]\n"
        "  mov sp, x10\n"
        "  br x11\n"
#ifndef __APPLE__
        ".size _CHIBA_co_asm_switch, .-_CHIBA_co_asm_switch\n"
#endif
);

__asm__(".text\n"
#ifdef __APPLE__
        ".globl __CHIBA_co_asm_entry\n"
        "__CHIBA_co_asm_entry:\n"
#else
        ".globl _CHIBA_co_asm_entry\n"
        ".type _CHIBA_co_asm_entry #function\n"
        ".hidden _CHIBA_co_asm_entry\n"
        "_CHIBA_co_asm_entry:\n"
#endif
        "  mov x0, x19\n"
        "  mov x30, x21\n"
        "  br x20\n"
#ifndef __APPLE__
        ".size _CHIBA_co_asm_entry, .-_CHIBA_co_asm_entry\n"
#endif
);

PRIVATE void CHIBA_co_asmctx_make(struct CHIBA_co_asmctx *ctx,
                                  anyptr stack_base, u64 stack_size,
                                  anyptr arg) {
  ctx->x[0] = arg;
  ctx->x[1] = (anyptr)(CHIBA_co_entry);
  ctx->x[2] = (anyptr)(0xdeaddeaddeaddead); /* Dummy return address. */
  ctx->sp = (anyptr)((u64)stack_base + stack_size);
  ctx->lr = (anyptr)(_CHIBA_co_asm_entry);
}

#endif // __aarch64__
