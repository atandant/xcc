/* SPDX-License-Identifier: MIT */
#ifndef XCC_TARGET_H
#define XCC_TARGET_H

/* Target abstraction seam (D3/D4/D15). Register allocation and lowering
 * reference only integer physical-register ids and this descriptor — never
 * AT&T register-name strings. */

typedef struct TargetDesc TargetDesc;

typedef enum {
    REG_CLASS_GPR,
    REG_CLASS_XMM,
    REG_CLASS_COUNT,
} RegClass;

struct TargetDesc {
    const char *name;
    int nalloc[REG_CLASS_COUNT];
    const int *alloc_order[REG_CLASS_COUNT];
    const char *(*reg_name)(int phys, int width);
    unsigned caller_saved_mask;
    unsigned callee_saved_mask;
    const int *arg_regs;
    int nargs_reg;
    int ret_reg;
    int div_num_reg;
    int div_rem_reg;
    int shift_count_reg;
    unsigned memcpy_clobber_mask;
    int scratch0;
    int scratch1;
    int imm_bits;
};

/* Physical register ids for x86-64 SysV (used in LIR pre-coloring). */
enum {
    PHYS_RAX = 0,
    PHYS_RDX,
    PHYS_RCX,
    PHYS_RBX,
    PHYS_RSI,
    PHYS_RDI,
    PHYS_R8,
    PHYS_R9,
    PHYS_R10,
    PHYS_R11,
    PHYS_R12,
    PHYS_R13,
    PHYS_R14,
    PHYS_R15,
    PHYS_XMM0,
    PHYS_XMM1,
    PHYS_XMM2,
    PHYS_XMM3,
    PHYS_XMM4,
    PHYS_XMM5,
    PHYS_XMM6,
    PHYS_XMM7,
    PHYS_XMM8,
    PHYS_XMM9,
    PHYS_XMM10,
    PHYS_XMM11,
    PHYS_XMM12,
    PHYS_XMM13,
    PHYS_XMM14,
    PHYS_XMM15,
    PHYS_COUNT
};

#define PHYS_NONE (-1)

extern const TargetDesc X86_SYSV;

/*
 * Adding a new target (D15)
 * -------------------------
 * 1. Add a TargetDesc in emit_<arch>.c (reg names, alloc pool, ABI masks).
 * 2. Wire codegen.c to pick the descriptor (today: always X86_SYSV).
 * 3. Implement emit_<arch>.c: prologue/epilogue, spill/reload, each LIR op.
 * 4. Lowering (lower.c) and liveness/regalloc use only TargetDesc + integer
 *    phys ids — no AT&T/asm strings outside emit_*.
 * 5. Run `rg -n '%r[a-z0-9]+' src/ | grep -v _x86` — must be empty.
 */

#endif /* XCC_TARGET_H */
