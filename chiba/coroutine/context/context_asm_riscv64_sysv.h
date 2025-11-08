// RISC-V 64-bit context switching implementation

#pragma once
#include "../../basic_types.h"

#if defined(__riscv) && !defined(CHIBA_CO_NOASM) && (__riscv_xlen == 64)
#define CHIBA_CO_ASM
#define CHIBA_CO_READY
#define CHIBA_CO_METHOD "asm,riscv64"
NOINLINE PRIVATE void CHIBA_co_entry(anyptr arg) __attribute__((noreturn));

struct CHIBA_co_asmctx {
  anyptr s[12]; /* s0-s11 */
  anyptr ra;
  anyptr pc;
  anyptr sp;
#ifdef __riscv_flen
#if __riscv_flen == 64
  f64 fs[12]; /* fs0-fs11 */
#endif
#endif /* __riscv_flen */
};

void _CHIBA_co_asm_entry(void);
i32 _CHIBA_co_asm_switch(struct CHIBA_co_asmctx *from,
                         struct CHIBA_co_asmctx *to);

__asm__(".text\n"
        ".globl _CHIBA_co_asm_entry\n"
        ".type _CHIBA_co_asm_entry @function\n"
        ".hidden _CHIBA_co_asm_entry\n"
        "_CHIBA_co_asm_entry:\n"
        "  mv a0, s0\n"
        "  jr s1\n"
        ".size _CHIBA_co_asm_entry, .-_CHIBA_co_asm_entry\n");

__asm__(".text\n"
        ".globl _CHIBA_co_asm_switch\n"
        ".type _CHIBA_co_asm_switch @function\n"
        ".hidden _CHIBA_co_asm_switch\n"
        "_CHIBA_co_asm_switch:\n"
        "  sd s0, 0x00(a0)\n"
        "  sd s1, 0x08(a0)\n"
        "  sd s2, 0x10(a0)\n"
        "  sd s3, 0x18(a0)\n"
        "  sd s4, 0x20(a0)\n"
        "  sd s5, 0x28(a0)\n"
        "  sd s6, 0x30(a0)\n"
        "  sd s7, 0x38(a0)\n"
        "  sd s8, 0x40(a0)\n"
        "  sd s9, 0x48(a0)\n"
        "  sd s10, 0x50(a0)\n"
        "  sd s11, 0x58(a0)\n"
        "  sd ra, 0x60(a0)\n"
        "  sd ra, 0x68(a0)\n" /* pc */
        "  sd sp, 0x70(a0)\n"
#ifdef __riscv_flen
#if __riscv_flen == 64
        "  fsd fs0, 0x78(a0)\n"
        "  fsd fs1, 0x80(a0)\n"
        "  fsd fs2, 0x88(a0)\n"
        "  fsd fs3, 0x90(a0)\n"
        "  fsd fs4, 0x98(a0)\n"
        "  fsd fs5, 0xa0(a0)\n"
        "  fsd fs6, 0xa8(a0)\n"
        "  fsd fs7, 0xb0(a0)\n"
        "  fsd fs8, 0xb8(a0)\n"
        "  fsd fs9, 0xc0(a0)\n"
        "  fsd fs10, 0xc8(a0)\n"
        "  fsd fs11, 0xd0(a0)\n"
        "  fld fs0, 0x78(a1)\n"
        "  fld fs1, 0x80(a1)\n"
        "  fld fs2, 0x88(a1)\n"
        "  fld fs3, 0x90(a1)\n"
        "  fld fs4, 0x98(a1)\n"
        "  fld fs5, 0xa0(a1)\n"
        "  fld fs6, 0xa8(a1)\n"
        "  fld fs7, 0xb0(a1)\n"
        "  fld fs8, 0xb8(a1)\n"
        "  fld fs9, 0xc0(a1)\n"
        "  fld fs10, 0xc8(a1)\n"
        "  fld fs11, 0xd0(a1)\n"
#endif
#endif /* __riscv_flen */
        "  ld s0, 0x00(a1)\n"
        "  ld s1, 0x08(a1)\n"
        "  ld s2, 0x10(a1)\n"
        "  ld s3, 0x18(a1)\n"
        "  ld s4, 0x20(a1)\n"
        "  ld s5, 0x28(a1)\n"
        "  ld s6, 0x30(a1)\n"
        "  ld s7, 0x38(a1)\n"
        "  ld s8, 0x40(a1)\n"
        "  ld s9, 0x48(a1)\n"
        "  ld s10, 0x50(a1)\n"
        "  ld s11, 0x58(a1)\n"
        "  ld ra, 0x60(a1)\n"
        "  ld a2, 0x68(a1)\n" /* pc */
        "  ld sp, 0x70(a1)\n"
        "  jr a2\n"
        ".size _CHIBA_co_asm_switch, .-_CHIBA_co_asm_switch\n");

// RISC-V 64-bit implementation
PRIVATE void CHIBA_co_asmctx_make(struct CHIBA_co_asmctx *ctx,
                                  anyptr stack_base, u64 stack_size,
                                  anyptr arg) {
  ctx->s[0] = arg;
  ctx->s[1] = (anyptr)(CHIBA_co_entry);
  ctx->pc = (anyptr)(_CHIBA_co_asm_entry);
  ctx->ra = (anyptr)(0xdeaddeaddeaddead);
  ctx->sp = (anyptr)((u64)stack_base + stack_size);
}

#endif // __riscv && __riscv_xlen == 64
