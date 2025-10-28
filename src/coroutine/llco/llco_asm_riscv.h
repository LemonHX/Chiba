#pragma once
#include "llco.h"

////////////////////////////////////////////////////////////////////////////////
// RISC-V (rv64/rv32)
////////////////////////////////////////////////////////////////////////////////
#if defined(__riscv) && !defined(LLCO_NOASM)
#define LLCO_ASM
#define LLCO_READY
#define LLCO_METHOD "asm,riscv"

struct llco_asmctx {
  void *s[12]; /* s0-s11 */
  void *ra;
  void *pc;
  void *sp;
#ifdef __riscv_flen
#if __riscv_flen == 64
  double fs[12]; /* fs0-fs11 */
#elif __riscv_flen == 32
  float fs[12]; /* fs0-fs11 */
#endif
#endif /* __riscv_flen */
};

// Entry point function
__attribute__((naked)) static inline void _llco_asm_entry(void) {
  __asm__ volatile("mv a0, s0\n\t"
                   "jr s1\n\t");
}

// Context switch function
__attribute__((naked)) static inline int
_llco_asm_switch(struct llco_asmctx *from, struct llco_asmctx *to) {
  __asm__ volatile(
#if __riscv_xlen == 64
      "sd s0, 0x00(a0)\n\t"
      "sd s1, 0x08(a0)\n\t"
      "sd s2, 0x10(a0)\n\t"
      "sd s3, 0x18(a0)\n\t"
      "sd s4, 0x20(a0)\n\t"
      "sd s5, 0x28(a0)\n\t"
      "sd s6, 0x30(a0)\n\t"
      "sd s7, 0x38(a0)\n\t"
      "sd s8, 0x40(a0)\n\t"
      "sd s9, 0x48(a0)\n\t"
      "sd s10, 0x50(a0)\n\t"
      "sd s11, 0x58(a0)\n\t"
      "sd ra, 0x60(a0)\n\t"
      "sd ra, 0x68(a0)\n\t" /* pc */
      "sd sp, 0x70(a0)\n\t"
#ifdef __riscv_flen
#if __riscv_flen == 64
      "fsd fs0, 0x78(a0)\n\t"
      "fsd fs1, 0x80(a0)\n\t"
      "fsd fs2, 0x88(a0)\n\t"
      "fsd fs3, 0x90(a0)\n\t"
      "fsd fs4, 0x98(a0)\n\t"
      "fsd fs5, 0xa0(a0)\n\t"
      "fsd fs6, 0xa8(a0)\n\t"
      "fsd fs7, 0xb0(a0)\n\t"
      "fsd fs8, 0xb8(a0)\n\t"
      "fsd fs9, 0xc0(a0)\n\t"
      "fsd fs10, 0xc8(a0)\n\t"
      "fsd fs11, 0xd0(a0)\n\t"
      "fld fs0, 0x78(a1)\n\t"
      "fld fs1, 0x80(a1)\n\t"
      "fld fs2, 0x88(a1)\n\t"
      "fld fs3, 0x90(a1)\n\t"
      "fld fs4, 0x98(a1)\n\t"
      "fld fs5, 0xa0(a1)\n\t"
      "fld fs6, 0xa8(a1)\n\t"
      "fld fs7, 0xb0(a1)\n\t"
      "fld fs8, 0xb8(a1)\n\t"
      "fld fs9, 0xc0(a1)\n\t"
      "fld fs10, 0xc8(a1)\n\t"
      "fld fs11, 0xd0(a1)\n\t"
#else
#error "Unsupported RISC-V FLEN"
#endif
#endif /* __riscv_flen */
      "ld s0, 0x00(a1)\n\t"
      "ld s1, 0x08(a1)\n\t"
      "ld s2, 0x10(a1)\n\t"
      "ld s3, 0x18(a1)\n\t"
      "ld s4, 0x20(a1)\n\t"
      "ld s5, 0x28(a1)\n\t"
      "ld s6, 0x30(a1)\n\t"
      "ld s7, 0x38(a1)\n\t"
      "ld s8, 0x40(a1)\n\t"
      "ld s9, 0x48(a1)\n\t"
      "ld s10, 0x50(a1)\n\t"
      "ld s11, 0x58(a1)\n\t"
      "ld ra, 0x60(a1)\n\t"
      "ld a2, 0x68(a1)\n\t" /* pc */
      "ld sp, 0x70(a1)\n\t"
      "jr a2\n\t"
#elif __riscv_xlen == 32
      "sw s0, 0x00(a0)\n\t"
      "sw s1, 0x04(a0)\n\t"
      "sw s2, 0x08(a0)\n\t"
      "sw s3, 0x0c(a0)\n\t"
      "sw s4, 0x10(a0)\n\t"
      "sw s5, 0x14(a0)\n\t"
      "sw s6, 0x18(a0)\n\t"
      "sw s7, 0x1c(a0)\n\t"
      "sw s8, 0x20(a0)\n\t"
      "sw s9, 0x24(a0)\n\t"
      "sw s10, 0x28(a0)\n\t"
      "sw s11, 0x2c(a0)\n\t"
      "sw ra, 0x30(a0)\n\t"
      "sw ra, 0x34(a0)\n\t" /* pc */
      "sw sp, 0x38(a0)\n\t"
#ifdef __riscv_flen
#if __riscv_flen == 64
      "fsd fs0, 0x3c(a0)\n\t"
      "fsd fs1, 0x44(a0)\n\t"
      "fsd fs2, 0x4c(a0)\n\t"
      "fsd fs3, 0x54(a0)\n\t"
      "fsd fs4, 0x5c(a0)\n\t"
      "fsd fs5, 0x64(a0)\n\t"
      "fsd fs6, 0x6c(a0)\n\t"
      "fsd fs7, 0x74(a0)\n\t"
      "fsd fs8, 0x7c(a0)\n\t"
      "fsd fs9, 0x84(a0)\n\t"
      "fsd fs10, 0x8c(a0)\n\t"
      "fsd fs11, 0x94(a0)\n\t"
      "fld fs0, 0x3c(a1)\n\t"
      "fld fs1, 0x44(a1)\n\t"
      "fld fs2, 0x4c(a1)\n\t"
      "fld fs3, 0x54(a1)\n\t"
      "fld fs4, 0x5c(a1)\n\t"
      "fld fs5, 0x64(a1)\n\t"
      "fld fs6, 0x6c(a1)\n\t"
      "fld fs7, 0x74(a1)\n\t"
      "fld fs8, 0x7c(a1)\n\t"
      "fld fs9, 0x84(a1)\n\t"
      "fld fs10, 0x8c(a1)\n\t"
      "fld fs11, 0x94(a1)\n\t"
#elif __riscv_flen == 32
      "fsw fs0, 0x3c(a0)\n\t"
      "fsw fs1, 0x40(a0)\n\t"
      "fsw fs2, 0x44(a0)\n\t"
      "fsw fs3, 0x48(a0)\n\t"
      "fsw fs4, 0x4c(a0)\n\t"
      "fsw fs5, 0x50(a0)\n\t"
      "fsw fs6, 0x54(a0)\n\t"
      "fsw fs7, 0x58(a0)\n\t"
      "fsw fs8, 0x5c(a0)\n\t"
      "fsw fs9, 0x60(a0)\n\t"
      "fsw fs10, 0x64(a0)\n\t"
      "fsw fs11, 0x68(a0)\n\t"
      "flw fs0, 0x3c(a1)\n\t"
      "flw fs1, 0x40(a1)\n\t"
      "flw fs2, 0x44(a1)\n\t"
      "flw fs3, 0x48(a1)\n\t"
      "flw fs4, 0x4c(a1)\n\t"
      "flw fs5, 0x50(a1)\n\t"
      "flw fs6, 0x54(a1)\n\t"
      "flw fs7, 0x58(a1)\n\t"
      "flw fs8, 0x5c(a1)\n\t"
      "flw fs9, 0x60(a1)\n\t"
      "flw fs10, 0x64(a1)\n\t"
      "flw fs11, 0x68(a1)\n\t"
#else
#error "Unsupported RISC-V FLEN"
#endif
#endif /* __riscv_flen */
      "lw s0, 0x00(a1)\n\t"
      "lw s1, 0x04(a1)\n\t"
      "lw s2, 0x08(a1)\n\t"
      "lw s3, 0x0c(a1)\n\t"
      "lw s4, 0x10(a1)\n\t"
      "lw s5, 0x14(a1)\n\t"
      "lw s6, 0x18(a1)\n\t"
      "lw s7, 0x1c(a1)\n\t"
      "lw s8, 0x20(a1)\n\t"
      "lw s9, 0x24(a1)\n\t"
      "lw s10, 0x28(a1)\n\t"
      "lw s11, 0x2c(a1)\n\t"
      "lw ra, 0x30(a1)\n\t"
      "lw a2, 0x34(a1)\n\t" /* pc */
      "lw sp, 0x38(a1)\n\t"
      "jr a2\n\t"
#else
#error "Unsupported RISC-V XLEN"
#endif /* __riscv_xlen */
  );
}

static void llco_asmctx_make(struct llco_asmctx *ctx, void *stack_base,
                             size_t stack_size, void *arg) {
  ctx->s[0] = (void *)(arg);
  ctx->s[1] = (void *)(llco_entry);
  ctx->pc = (void *)(_llco_asm_entry);
#if __riscv_xlen == 64
  ctx->ra = (void *)(0xdeaddeaddeaddead);
#elif __riscv_xlen == 32
  ctx->ra = (void *)(0xdeaddead);
#endif
  ctx->sp = (void *)((size_t)stack_base + stack_size);
}

#endif // __riscv && !LLCO_NOASM
