#pragma once
#include "llco.h"

////////////////////////////////////////////////////////////////////////////////
// ARM 64-bit (AArch64)
////////////////////////////////////////////////////////////////////////////////
#if defined(__aarch64__) && !defined(LLCO_NOASM)
#define LLCO_ASM
#define LLCO_READY
#define LLCO_METHOD "asm,aarch64"

struct llco_asmctx {
  void *x[12]; /* x19-x30 */
  void *sp;
  void *lr;
  void *d[8]; /* d8-d15 */
};

// Context switch function
__attribute__((naked)) static inline int
_llco_asm_switch(struct llco_asmctx *from, struct llco_asmctx *to) {
  __asm__ volatile("mov x10, sp\n\t"
                   "mov x11, x30\n\t"
                   "stp x19, x20, [x0, #(0*16)]\n\t"
                   "stp x21, x22, [x0, #(1*16)]\n\t"
                   "stp d8, d9, [x0, #(7*16)]\n\t"
                   "stp x23, x24, [x0, #(2*16)]\n\t"
                   "stp d10, d11, [x0, #(8*16)]\n\t"
                   "stp x25, x26, [x0, #(3*16)]\n\t"
                   "stp d12, d13, [x0, #(9*16)]\n\t"
                   "stp x27, x28, [x0, #(4*16)]\n\t"
                   "stp d14, d15, [x0, #(10*16)]\n\t"
                   "stp x29, x30, [x0, #(5*16)]\n\t"
                   "stp x10, x11, [x0, #(6*16)]\n\t"
                   "ldp x19, x20, [x1, #(0*16)]\n\t"
                   "ldp x21, x22, [x1, #(1*16)]\n\t"
                   "ldp d8, d9, [x1, #(7*16)]\n\t"
                   "ldp x23, x24, [x1, #(2*16)]\n\t"
                   "ldp d10, d11, [x1, #(8*16)]\n\t"
                   "ldp x25, x26, [x1, #(3*16)]\n\t"
                   "ldp d12, d13, [x1, #(9*16)]\n\t"
                   "ldp x27, x28, [x1, #(4*16)]\n\t"
                   "ldp d14, d15, [x1, #(10*16)]\n\t"
                   "ldp x29, x30, [x1, #(5*16)]\n\t"
                   "ldp x10, x11, [x1, #(6*16)]\n\t"
                   "mov sp, x10\n\t"
                   "br x11\n\t");
}

// Entry point function
__attribute__((naked)) static inline void _llco_asm_entry(void) {
  __asm__ volatile("mov x0, x19\n\t"
                   "mov x30, x21\n\t"
                   "br x20\n\t");
}

static void llco_asmctx_make(struct llco_asmctx *ctx, void *stack_base,
                             size_t stack_size, void *arg) {
  ctx->x[0] = (void *)(arg);
  ctx->x[1] = (void *)(llco_entry);
  ctx->x[2] = (void *)(0xdeaddeaddeaddead); /* Dummy return address. */
  ctx->sp = (void *)((size_t)stack_base + stack_size);
  ctx->lr = (void *)(_llco_asm_entry);
}

#endif // __aarch64__ && !LLCO_NOASM
