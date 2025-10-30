// x86-64 System V ABI context switching implementation (Unix/Linux/macOS)
// Adapted from llco (https://github.com/tidwall/llco)
// Copyright (c) 2024 Joshua J Baker.

#pragma once
#include "../../basic_types.h"

#if (defined(__x86_64__) || defined(_M_X64)) && !defined(CHIBA_CO_NOASM) &&    \
    !defined(_WIN32)
#define CHIBA_CO_ASM
#define CHIBA_CO_READY
#define CHIBA_CO_METHOD "asm,x64_sysv"

struct CHIBA_co_asmctx {
  anyptr rip, rsp, rbp, rbx, r12, r13, r14, r15;
};

void _CHIBA_co_asm_entry(void);
i32 _CHIBA_co_asm_switch(struct CHIBA_co_asmctx *from,
                         struct CHIBA_co_asmctx *to);

__asm__(".text\n"
#ifdef __MACH__ /* Mac OS X assembler */
        ".globl __CHIBA_co_asm_entry\n"
        "__CHIBA_co_asm_entry:\n"
#else /* Linux assembler */
        ".globl _CHIBA_co_asm_entry\n"
        ".type _CHIBA_co_asm_entry @function\n"
        ".hidden _CHIBA_co_asm_entry\n"
        "_CHIBA_co_asm_entry:\n"
#endif
        "  movq %r13, %rdi\n"
        "  jmpq *%r12\n"
#ifndef __MACH__
        ".size _CHIBA_co_asm_entry, .-_CHIBA_co_asm_entry\n"
#endif
);

__asm__(".text\n"
#ifdef __MACH__ /* Mac OS assembler */
        ".globl __CHIBA_co_asm_switch\n"
        "__CHIBA_co_asm_switch:\n"
#else /* Linux assembler */
        ".globl _CHIBA_co_asm_switch\n"
        ".type _CHIBA_co_asm_switch @function\n"
        ".hidden _CHIBA_co_asm_switch\n"
        "_CHIBA_co_asm_switch:\n"
#endif
        "  leaq 0x3d(%rip), %rax\n"
        "  movq %rax, (%rdi)\n"
        "  movq %rsp, 8(%rdi)\n"
        "  movq %rbp, 16(%rdi)\n"
        "  movq %rbx, 24(%rdi)\n"
        "  movq %r12, 32(%rdi)\n"
        "  movq %r13, 40(%rdi)\n"
        "  movq %r14, 48(%rdi)\n"
        "  movq %r15, 56(%rdi)\n"
        "  movq 56(%rsi), %r15\n"
        "  movq 48(%rsi), %r14\n"
        "  movq 40(%rsi), %r13\n"
        "  movq 32(%rsi), %r12\n"
        "  movq 24(%rsi), %rbx\n"
        "  movq 16(%rsi), %rbp\n"
        "  movq 8(%rsi), %rsp\n"
        "  jmpq *(%rsi)\n"
        "  ret\n"
#ifndef __MACH__
        ".size _CHIBA_co_asm_switch, .-_CHIBA_co_asm_switch\n"
#endif
);

// System V x64 implementation (Unix/Linux/macOS)
PRIVATE void CHIBA_co_asmctx_make(struct CHIBA_co_asmctx *ctx,
                                  anyptr stack_base, u64 stack_size,
                                  anyptr arg) {
  // Reserve 128 bytes for the Red Zone space (System V AMD64 ABI).
  stack_size = stack_size - 128;
  anyptr *stack_high_ptr =
      (anyptr *)((u64)stack_base + stack_size - sizeof(u64));
  stack_high_ptr[0] = (anyptr)(0xdeaddeaddeaddead); // Dummy return address.
  ctx->rip = (anyptr)(_CHIBA_co_asm_entry);
  ctx->rsp = (anyptr)(stack_high_ptr);
  ctx->r12 = (anyptr)(CHIBA_co_entry);
  ctx->r13 = arg;
}

#endif // __x86_64__ && !_WIN32
