#pragma once
#include "llco.h"

////////////////////////////////////////////////////////////////////////////////
// x64 System V ABI (POSIX: macOS, Linux, BSD)
////////////////////////////////////////////////////////////////////////////////
#if (defined(__x86_64__) || defined(_M_X64)) && !defined(_WIN32) &&            \
    !defined(LLCO_NOASM)
#define LLCO_ASM
#define LLCO_READY
#define LLCO_METHOD "asm,x64"

struct llco_asmctx {
  void *rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15;
};

// Entry point function
__attribute__((naked)) static inline void _llco_asm_entry(void) {
  __asm__ volatile("movq %r13, %rdi\n\t"
                   "jmpq *%r12\n\t");
}

// Context switch function
__attribute__((naked)) static inline int
_llco_asm_switch(struct llco_asmctx *from, struct llco_asmctx *to) {
  __asm__ volatile("leaq 0x3d(%rip), %rax\n\t"
                   "movq %rax, (%rdi)\n\t"
                   "movq %rsp, 8(%rdi)\n\t"
                   "movq %rbp, 16(%rdi)\n\t"
                   "movq %rbx, 24(%rdi)\n\t"
                   "movq %r12, 32(%rdi)\n\t"
                   "movq %r13, 40(%rdi)\n\t"
                   "movq %r14, 48(%rdi)\n\t"
                   "movq %r15, 56(%rdi)\n\t"
                   "movq 56(%rsi), %r15\n\t"
                   "movq 48(%rsi), %r14\n\t"
                   "movq 40(%rsi), %r13\n\t"
                   "movq 32(%rsi), %r12\n\t"
                   "movq 24(%rsi), %rbx\n\t"
                   "movq 16(%rsi), %rbp\n\t"
                   "movq 8(%rsi), %rsp\n\t"
                   "jmpq *(%rsi)\n\t"
                   "ret\n\t");
}

static void llco_asmctx_make(struct llco_asmctx *ctx, void *stack_base,
                             size_t stack_size, void *arg) {
  // Reserve 128 bytes for the Red Zone space (System V AMD64 ABI).
  stack_size = stack_size - 128;
  void **stack_high_ptr =
      (void **)((size_t)stack_base + stack_size - sizeof(size_t));
  stack_high_ptr[0] = (void *)(0xdeaddeaddeaddead); // Dummy return address.
  ctx->rip = (void *)(_llco_asm_entry);
  ctx->rsp = (void *)(stack_high_ptr);
  ctx->r12 = (void *)(llco_entry);
  ctx->r13 = (void *)(arg);
}

#endif // x64 && !_WIN32 && !LLCO_NOASM
