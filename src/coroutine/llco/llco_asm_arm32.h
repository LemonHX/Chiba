#pragma once
#include "llco.h"

////////////////////////////////////////////////////////////////////////////////
// ARM 32-bit (ARM EABI)
////////////////////////////////////////////////////////////////////////////////
#if defined(__ARM_EABI__) && !defined(LLCO_NOASM)
#define LLCO_ASM
#define LLCO_READY
#define LLCO_METHOD "asm,arm_eabi"

struct llco_asmctx {
#ifndef __SOFTFP__
  void *f[16];
#endif
  void *d[4]; /* d8-d15 */
  void *r[4]; /* r4-r11 */
  void *lr;
  void *sp;
};

// Context switch function
__attribute__((naked)) static inline void
_llco_asm_switch(struct llco_asmctx *from, struct llco_asmctx *to) {
  __asm__ volatile(
#ifndef __SOFTFP__
      "vstmia r0!, {d8-d15}\n\t"
#endif
      "stmia r0, {r4-r11, lr}\n\t"
      "str sp, [r0, #9*4]\n\t"
#ifndef __SOFTFP__
      "vldmia r1!, {d8-d15}\n\t"
#endif
      "ldr sp, [r1, #9*4]\n\t"
      "ldmia r1, {r4-r11, pc}\n\t");
}

// Entry point function
__attribute__((naked)) static inline void _llco_asm_entry(void) {
  __asm__ volatile("mov r0, r4\n\t"
                   "mov ip, r5\n\t"
                   "mov lr, r6\n\t"
                   "bx ip\n\t");
}

static void llco_asmctx_make(struct llco_asmctx *ctx, void *stack_base,
                             size_t stack_size, void *arg) {
  ctx->d[0] = (void *)(arg);
  ctx->d[1] = (void *)(llco_entry);
  ctx->d[2] = (void *)(0xdeaddead); /* Dummy return address. */
  ctx->lr = (void *)(_llco_asm_entry);
  ctx->sp = (void *)((size_t)stack_base + stack_size);
}

#endif // __ARM_EABI__ && !LLCO_NOASM
