#pragma once
#include "llco.h"

////////////////////////////////////////////////////////////////////////////////
// x86 (i386)
////////////////////////////////////////////////////////////////////////////////
#if (defined(__i386) || defined(__i386__)) && !defined(LLCO_NOASM)
#define LLCO_ASM
#define LLCO_READY
#define LLCO_METHOD "asm,i386"

struct llco_asmctx {
  void *eip, *esp, *ebp, *ebx, *esi, *edi;
};

// Context switch function
__attribute__((naked)) static inline void
_llco_asm_switch(struct llco_asmctx *from, struct llco_asmctx *to) {
  __asm__ volatile("call 1f\n\t"
                   "1:\n\t"
                   "popl %ecx\n\t"
                   "addl $(2f-1b), %ecx\n\t"
                   "movl 4(%esp), %eax\n\t"
                   "movl 8(%esp), %edx\n\t"
                   "movl %ecx, (%eax)\n\t"
                   "movl %esp, 4(%eax)\n\t"
                   "movl %ebp, 8(%eax)\n\t"
                   "movl %ebx, 12(%eax)\n\t"
                   "movl %esi, 16(%eax)\n\t"
                   "movl %edi, 20(%eax)\n\t"
                   "movl 20(%edx), %edi\n\t"
                   "movl 16(%edx), %esi\n\t"
                   "movl 12(%edx), %ebx\n\t"
                   "movl 8(%edx), %ebp\n\t"
                   "movl 4(%edx), %esp\n\t"
                   "jmp *(%edx)\n\t"
                   "2:\n\t"
                   "ret\n\t");
}

static void llco_asmctx_make(struct llco_asmctx *ctx, void *stack_base,
                             size_t stack_size, void *arg) {
  void **stack_high_ptr =
      (void **)((size_t)stack_base + stack_size - 16 - 1 * sizeof(size_t));
  stack_high_ptr[0] = (void *)(0xdeaddead); // Dummy return address.
  stack_high_ptr[1] = (void *)(arg);
  ctx->eip = (void *)(llco_entry);
  ctx->esp = (void *)(stack_high_ptr);
}

#endif // __i386__ && !LLCO_NOASM
