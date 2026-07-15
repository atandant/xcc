/* SPDX-License-Identifier: MIT */
#include "target.h"
#include "lir.h"
#include "regalloc.h"
#include "type.h"
#include "abi_sysv_amd64.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static const char *reg64_name(int phys)
{
    static const char *names[PHYS_COUNT] = {
        "%rax", "%rdx", "%rcx", "%rbx", "%rsi", "%rdi",
        "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15",
    };
    assert(phys >= 0 && phys < PHYS_COUNT);
    return names[phys];
}

static const char *reg32_name(int phys)
{
    static const char *names[PHYS_COUNT] = {
        "%eax", "%edx", "%ecx", "%ebx", "%esi", "%edi",
        "%r8d", "%r9d", "%r10d", "%r11d", "%r12d", "%r13d", "%r14d", "%r15d",
    };
    return names[phys];
}

static const char *reg16_name(int phys)
{
    static const char *names[PHYS_COUNT] = {
        "%ax", "%dx", "%cx", "%bx", "%si", "%di",
        "%r8w", "%r9w", "%r10w", "%r11w", "%r12w", "%r13w", "%r14w", "%r15w",
    };
    return names[phys];
}

static const char *reg8_name(int phys)
{
    static const char *names[PHYS_COUNT] = {
        "%al", "%dl", "%cl", "%bl", "%sil", "%dil",
        "%r8b", "%r9b", "%r10b", "%r11b", "%r12b", "%r13b", "%r14b", "%r15b",
    };
    return names[phys];
}

static const char *x86_reg_name(int phys, int width)
{
    if (width == 1)
        return reg8_name(phys);
    if (width == 2)
        return reg16_name(phys);
    if (width == 4)
        return reg32_name(phys);
    return reg64_name(phys);
}

static const char *x86_reg_name_lir(int phys, LirWidth w)
{
    return x86_reg_name(phys, w == LIR_W4 ? 4 : 8);
}

static const char *x86_reg_name_fn(int phys, int width)
{
  (void)width;
  return x86_reg_name(phys, 8);
}

static const int x86_alloc_order[] = {
    PHYS_R8, PHYS_R9, PHYS_RCX, PHYS_RDX,
    PHYS_RBX, PHYS_R12, PHYS_R13, PHYS_R14, PHYS_R15,
};

static const int x86_arg_regs[] = {
    PHYS_RDI, PHYS_RSI, PHYS_RDX, PHYS_RCX, PHYS_R8, PHYS_R9,
};

#define X86_CALLER_SAVED_MASK \
    ((1u << PHYS_RAX) | (1u << PHYS_RCX) | (1u << PHYS_RDX) | \
     (1u << PHYS_RSI) | (1u << PHYS_RDI) | (1u << PHYS_R8) | \
     (1u << PHYS_R9) | (1u << PHYS_R10) | (1u << PHYS_R11))

#define X86_CALLEE_SAVED_MASK \
    ((1u << PHYS_RBX) | (1u << PHYS_R12) | (1u << PHYS_R13) | \
     (1u << PHYS_R14) | (1u << PHYS_R15))

const TargetDesc X86_SYSV = {
    .name = "x86-64-sysv",
    .nalloc = 9,
    .alloc_order = x86_alloc_order,
    .reg_name = x86_reg_name_fn,
    .caller_saved_mask = X86_CALLER_SAVED_MASK,
    .callee_saved_mask = X86_CALLEE_SAVED_MASK,
    .arg_regs = x86_arg_regs,
    .nargs_reg = 6,
    .ret_reg = PHYS_RAX,
    .div_num_reg = PHYS_RAX,
    .div_rem_reg = PHYS_RDX,
    .memcpy_clobber_mask = (1u << PHYS_RAX) | (1u << PHYS_RCX) |
                           (1u << PHYS_RSI) | (1u << PHYS_RDI),
    .scratch0 = PHYS_R10,
    .scratch1 = PHYS_R11,
    .imm_bits = 32,
};

typedef struct {
    FILE *out;
    LirFn *lf;
    Function *fn;
    AllocResult *alloc;
    const TargetDesc *td;
    int rax_vreg;
} EmitCtx;

static void invalidate_rax(EmitCtx *c)
{
    c->rax_vreg = -1;
}

static int vreg_phys(EmitCtx *c, int v)
{
    if (v < 0)
        return REG_NONE;
    return c->alloc->vreg_reg[v];
}

static int callee_saved_count(AllocResult *alloc)
{
    int n = 0;

    for (int i = 0; i < PHYS_COUNT; i++) {
        if (alloc->used_callee_saved & (1u << i))
            n++;
    }
    return n;
}

static int callee_save_area(AllocResult *alloc)
{
    int n = callee_saved_count(alloc);

    return n * 8 + (n & 1 ? 8 : 0);
}

static long fp_disp(EmitCtx *c, long disp)
{
    if (disp <= (long)INT_MIN + 1)
        return disp;
    if (disp < 0)
        return disp - callee_save_area(c->alloc);
    return disp;
}

static int vreg_off(EmitCtx *c, int v)
{
    return (int)fp_disp(c, c->alloc->vreg_off[v]);
}

static int dst_phys(EmitCtx *c, int dst, int *phys)
{
    int p = vreg_phys(c, dst);
    if (p < 0)
        return 0;
    *phys = p;
    return 1;
}

static void materialize_vreg(EmitCtx *c, int v, const char *reg)
{
    if (strcmp(reg, "%rax") == 0 && c->rax_vreg == v)
        return;

    int p = c->alloc->vreg_reg[v];
    if (p >= 0) {
        const char *pn = reg64_name(p);
        if (strcmp(pn, reg) != 0)
            fprintf(c->out, "  mov %s, %s\n", pn, reg);
    } else {
        fprintf(c->out, "  mov %d(%%rbp), %s\n", vreg_off(c, v), reg);
    }

    if (strcmp(reg, "%rax") == 0)
        c->rax_vreg = v;
    else if (c->rax_vreg == v)
        c->rax_vreg = -1;
}

static void store_vreg_value(EmitCtx *c, int v, const char *reg)
{
    int p = c->alloc->vreg_reg[v];
    if (p >= 0) {
        const char *pn = reg64_name(p);
        if (strcmp(pn, reg) != 0)
            fprintf(c->out, "  mov %s, %s\n", reg, pn);
    } else {
        fprintf(c->out, "  mov %s, %d(%%rbp)\n", reg, vreg_off(c, v));
    }

    if (strcmp(reg, "%rax") == 0)
        c->rax_vreg = v;
    else if (c->rax_vreg == v)
        c->rax_vreg = -1;
}

static int spilled_vreg_off(EmitCtx *c, Operand op)
{
    if (op.kind != OPND_VREG)
        return INT_MAX;
    int v = op.u.vreg;
    if (c->alloc->vreg_reg[v] >= 0)
        return INT_MAX;
    return vreg_off(c, v);
}

static void emit_vreg_slot(EmitCtx *c, int v, const char *reg)
{
    materialize_vreg(c, v, reg);
}

static void store_vreg_slot(EmitCtx *c, int v, const char *reg)
{
    store_vreg_value(c, v, reg);
}

static void load_mem_addr(EmitCtx *c, Operand mem, const char *reg)
{
    if (mem.u.mem.base == LIR_FP) {
        long disp = fp_disp(c, mem.u.mem.disp);

        if (mem.u.mem.index == LIR_NO_IDX) {
            fprintf(c->out, "  lea %ld(%%rbp), %s\n", disp, reg);
            return;
        }
        emit_vreg_slot(c, mem.u.mem.index, "%r11");
        fprintf(c->out, "  lea %ld(%%rbp,%%r11,%d), %s\n",
                disp, mem.u.mem.scale, reg);
        return;
    }

    if (mem.u.mem.index == LIR_NO_IDX && mem.u.mem.disp == 0) {
        /* base + 0: load the base directly into the target register. */
        emit_vreg_slot(c, mem.u.mem.base, reg);
        return;
    }

    emit_vreg_slot(c, mem.u.mem.base, "%r10");
    if (mem.u.mem.index == LIR_NO_IDX) {
        fprintf(c->out, "  lea %ld(%%r10), %s\n", mem.u.mem.disp, reg);
        return;
    }
    emit_vreg_slot(c, mem.u.mem.index, "%r11");
    fprintf(c->out, "  lea (%%r10,%%r11,%d), %s\n", mem.u.mem.scale, reg);
    if (mem.u.mem.disp)
        fprintf(c->out, "  lea %ld(%s), %s\n", mem.u.mem.disp, reg, reg);
}

static void load_operand(EmitCtx *c, Operand op, const char *reg, LirWidth w)
{
    const char *r = x86_reg_name_lir(PHYS_RAX, w);
    (void)r;

    switch (op.kind) {
    case OPND_VREG:
        materialize_vreg(c, op.u.vreg, reg);
        return;
    case OPND_IMM:
        fprintf(c->out, "  mov $%ld, %s\n", op.u.imm, reg);
        return;
    case OPND_PHYS:
        fprintf(c->out, "  mov %s, %s\n",
                x86_reg_name_lir(op.u.phys, w), reg);
        return;
    case OPND_MEM:
        load_mem_addr(c, op, "%r10");
        switch (w) {
        case LIR_W4:
            fprintf(c->out, "  movslq (%%r10), %s\n", reg);
            return;
        case LIR_W8:
            fprintf(c->out, "  mov (%%r10), %s\n", reg);
            return;
        }
        return;
    default:
        assert(0);
    }
}

static void emit_label_ref(EmitCtx *c, int id)
{
    if (id == c->lf->epilogue_label)
        fprintf(c->out, ".L.return.%s", c->fn->name);
    else
        fprintf(c->out, ".L.%s.%d", c->fn->name, id);
}

static const char *jcc_for(LirCond cc)
{
    switch (cc) {
    case CC_EQ: return "je";
    case CC_NE: return "jne";
    case CC_LT: return "jl";
    case CC_LE: return "jle";
    case CC_GT: return "jg";
    case CC_GE: return "jge";
    }
    return "je";
}

static const char *jcc_for_sign(LirCond cc, LirSign sgn)
{
    if (sgn == LIR_SGN_U) {
        switch (cc) {
        case CC_EQ: return "je";
        case CC_NE: return "jne";
        case CC_LT: return "jb";
        case CC_LE: return "jbe";
        case CC_GT: return "ja";
        case CC_GE: return "jae";
        }
    }
    return jcc_for(cc);
}

static const char *setcc_for(LirCond cc)
{
    switch (cc) {
    case CC_EQ: return "sete";
    case CC_NE: return "setne";
    case CC_LT: return "setl";
    case CC_LE: return "setle";
    case CC_GT: return "setg";
    case CC_GE: return "setge";
    }
    return "sete";
}

static const char *setcc_for_sign(LirCond cc, LirSign sgn)
{
    if (sgn == LIR_SGN_U) {
        switch (cc) {
        case CC_EQ: return "sete";
        case CC_NE: return "setne";
        case CC_LT: return "setb";
        case CC_LE: return "setbe";
        case CC_GT: return "seta";
        case CC_GE: return "setae";
        }
    }
    return setcc_for(cc);
}

/* Returns a register name holding operand `a` for use as the cmp/test
   destination.  If `a` is already in a physical register it is used in place;
   otherwise it is loaded into `scratch`. */
static const char *cmp_lhs_reg(EmitCtx *c, Operand op, LirWidth w, int scratch)
{
    int phys = -1;
    if (op.kind == OPND_VREG)
        phys = c->alloc->vreg_reg[op.u.vreg];
    else if (op.kind == OPND_PHYS)
        phys = op.u.phys;

    if (phys >= 0)
        return x86_reg_name_lir(phys, w);

    load_operand(c, op, reg64_name(scratch), w);
    return x86_reg_name_lir(scratch, w);
}

/* Emits a `cmp`/`test` comparing a against b (flags reflect a - b), keeping
   operands in place where possible instead of funnelling through scratch. */
static void emit_cmp(EmitCtx *c, Operand a, Operand b, LirWidth w)
{
    int s0 = c->td->scratch0;
    int s1 = c->td->scratch1;

    /* Comparison against zero collapses to `test`, avoiding materializing 0. */
    if (b.kind == OPND_IMM && b.u.imm == 0) {
        const char *ar = cmp_lhs_reg(c, a, w, s0);
        fprintf(c->out, "  test %s, %s\n", ar, ar);
        return;
    }

    const char *ar = cmp_lhs_reg(c, a, w, s0);

    if (b.kind == OPND_IMM) {
        fprintf(c->out, "  cmp $%ld, %s\n", b.u.imm, ar);
        return;
    }

    int bphys = -1;
    if (b.kind == OPND_VREG)
        bphys = c->alloc->vreg_reg[b.u.vreg];
    else if (b.kind == OPND_PHYS)
        bphys = b.u.phys;

    if (bphys >= 0) {
        fprintf(c->out, "  cmp %s, %s\n", x86_reg_name_lir(bphys, w), ar);
        return;
    }

    if (b.kind == OPND_VREG) {
        int off = vreg_off(c, b.u.vreg);
        if (w == LIR_W4)
            fprintf(c->out, "  cmpl %d(%%rbp), %s\n", off, ar);
        else
            fprintf(c->out, "  cmpq %d(%%rbp), %s\n", off, ar);
        return;
    }

    load_operand(c, b, reg64_name(s1), w);
    fprintf(c->out, "  cmp %s, %s\n", x86_reg_name_lir(s1, w), ar);
}

static void emit_store_rax_partial(EmitCtx *c, const char *base, long off,
                                   int bytes)
{
    if (bytes >= 4) {
        fprintf(c->out, "  mov %%eax, %ld(%s)\n", off, base);
        fprintf(c->out, "  shr $32, %%rax\n");
        off += 4;
        bytes -= 4;
    }
    if (bytes >= 2) {
        fprintf(c->out, "  mov %%ax, %ld(%s)\n", off, base);
        fprintf(c->out, "  shr $16, %%rax\n");
        off += 2;
        bytes -= 2;
    }
    if (bytes == 1)
        fprintf(c->out, "  mov %%al, %ld(%s)\n", off, base);
}

static void emit_load_rax_partial(EmitCtx *c, const char *base, long off,
                                  int bytes)
{
    int done = 0;

    fprintf(c->out, "  xor %%eax, %%eax\n");
    if (bytes >= 4) {
        fprintf(c->out, "  mov %ld(%s), %%eax\n", off, base);
        done = 4;
    } else if (bytes >= 2) {
        fprintf(c->out, "  movzwl %ld(%s), %%eax\n", off, base);
        done = 2;
    } else if (bytes == 1) {
        fprintf(c->out, "  movzbl %ld(%s), %%eax\n", off, base);
        return;
    }
    if (bytes - done >= 2) {
        fprintf(c->out, "  movzwl %ld(%s), %%r11d\n", off + done, base);
        fprintf(c->out, "  shl $%d, %%r11\n", done * 8);
        fprintf(c->out, "  or %%r11, %%rax\n");
        done += 2;
    }
    if (bytes - done == 1) {
        fprintf(c->out, "  movzbl %ld(%s), %%r11d\n", off + done, base);
        fprintf(c->out, "  shl $%d, %%r11\n", done * 8);
        fprintf(c->out, "  or %%r11, %%rax\n");
    }
}

static void emit_store_mem(EmitCtx *c, Operand mem, LirWidth w, int bytes)
{
    load_mem_addr(c, mem, "%r10");
    switch (bytes) {
    case 1:
        fprintf(c->out, "  mov %%al, (%%r10)\n");
        return;
    case 2:
        fprintf(c->out, "  mov %%ax, (%%r10)\n");
        return;
    case 4:
        fprintf(c->out, "  mov %%eax, (%%r10)\n");
        return;
    case 8:
        fprintf(c->out, "  mov %%rax, (%%r10)\n");
        (void)w;
        return;
    default:
        emit_store_rax_partial(c, "%r10", 0, bytes);
        return;
    }
}

static void emit_load_fp_slot(EmitCtx *c, Operand mem, int bytes, LirSign sgn)
{
    long off = fp_disp(c, mem.u.mem.disp);
    switch (bytes) {
    case 1:
        if (sgn == LIR_SGN_S)
            fprintf(c->out, "  movsbl %ld(%%rbp), %%eax\n", off);
        else
            fprintf(c->out, "  movzbl %ld(%%rbp), %%eax\n", off);
        return;
    case 2:
        if (sgn == LIR_SGN_S)
            fprintf(c->out, "  movswl %ld(%%rbp), %%eax\n", off);
        else
            fprintf(c->out, "  movzwl %ld(%%rbp), %%eax\n", off);
        return;
    case 4:
        fprintf(c->out, "  movslq %ld(%%rbp), %%rax\n", off);
        return;
    case 8:
        fprintf(c->out, "  mov %ld(%%rbp), %%rax\n", off);
        return;
    default:
        emit_load_rax_partial(c, "%rbp", off, bytes);
        return;
    }
}

static void emit_store_fp_slot(EmitCtx *c, Operand mem, int bytes)
{
    long off = fp_disp(c, mem.u.mem.disp);
    switch (bytes) {
    case 1:
        fprintf(c->out, "  mov %%al, %ld(%%rbp)\n", off);
        return;
    case 2:
        fprintf(c->out, "  mov %%ax, %ld(%%rbp)\n", off);
        return;
    case 4:
        fprintf(c->out, "  mov %%eax, %ld(%%rbp)\n", off);
        return;
    case 8:
        fprintf(c->out, "  mov %%rax, %ld(%%rbp)\n", off);
        return;
    default:
        emit_store_rax_partial(c, "%rbp", off, bytes);
        return;
    }
}

static void emit_w4_result(EmitCtx *c, int phys, const char *scratch)
{
    if (phys >= 0)
        fprintf(c->out, "  movslq %s, %s\n", reg32_name(phys), reg64_name(phys));
    else
        fprintf(c->out, "  movslq %s, %%rax\n", scratch);
}

static void emit_pow2_store(EmitCtx *c, Instr *ins)
{
    store_vreg_slot(c, ins->dst, "%rax");
    invalidate_rax(c);
}

static void emit_umod_pow2(EmitCtx *c, Instr *ins)
{
    long mask = (1L << ins->aux) - 1L;

    load_operand(c, ins->a, "%rax", ins->w);
    if (ins->w == LIR_W4) {
        fprintf(c->out, "  and $%ld, %%eax\n", mask);
        fprintf(c->out, "  cltq\n");
    } else {
        fprintf(c->out, "  and $%ld, %%rax\n", mask);
    }
    emit_pow2_store(c, ins);
}

static void emit_udiv_pow2(EmitCtx *c, Instr *ins)
{
    load_operand(c, ins->a, "%rax", ins->w);
    if (ins->w == LIR_W4) {
        fprintf(c->out, "  shr $%d, %%eax\n", ins->aux);
        fprintf(c->out, "  cltq\n");
    } else {
        fprintf(c->out, "  shr $%d, %%rax\n", ins->aux);
    }
    emit_pow2_store(c, ins);
}

static void emit_smod_pow2(EmitCtx *c, Instr *ins)
{
    long mask = (1L << ins->aux) - 1L;
    const char *tmp = reg64_name(c->td->scratch0);
    const char *tmp32 = reg32_name(c->td->scratch0);

    load_operand(c, ins->a, "%rax", ins->w);
    if (ins->w == LIR_W4) {
        fprintf(c->out, "  mov %%eax, %s\n", tmp32);
        fprintf(c->out, "  sar $31, %s\n", tmp32);
        fprintf(c->out, "  and $%ld, %s\n", mask, tmp32);
        fprintf(c->out, "  lea (%%rax,%s), %%eax\n", tmp);
        fprintf(c->out, "  and $%ld, %%eax\n", mask);
        fprintf(c->out, "  sub %s, %%eax\n", tmp32);
        fprintf(c->out, "  cltq\n");
    } else {
        fprintf(c->out, "  mov %%rax, %s\n", tmp);
        fprintf(c->out, "  sar $63, %s\n", tmp);
        fprintf(c->out, "  and $%ld, %s\n", mask, tmp);
        fprintf(c->out, "  lea (%%rax,%s), %%rax\n", tmp);
        fprintf(c->out, "  and $%ld, %%rax\n", mask);
        fprintf(c->out, "  sub %s, %%rax\n", tmp);
    }
    emit_pow2_store(c, ins);
}

static void emit_sdiv_pow2(EmitCtx *c, Instr *ins)
{
    long mask = (1L << ins->aux) - 1L;
    const char *tmp = reg64_name(c->td->scratch0);
    const char *tmp32 = reg32_name(c->td->scratch0);

    load_operand(c, ins->a, "%rax", ins->w);
    if (ins->w == LIR_W4) {
        fprintf(c->out, "  mov %%eax, %s\n", tmp32);
        fprintf(c->out, "  sar $31, %s\n", tmp32);
        fprintf(c->out, "  and $%ld, %s\n", mask, tmp32);
        fprintf(c->out, "  lea (%%rax,%s), %%eax\n", tmp);
        fprintf(c->out, "  sar $%d, %%eax\n", ins->aux);
        fprintf(c->out, "  cltq\n");
    } else {
        fprintf(c->out, "  mov %%rax, %s\n", tmp);
        fprintf(c->out, "  sar $63, %s\n", tmp);
        fprintf(c->out, "  and $%ld, %s\n", mask, tmp);
        fprintf(c->out, "  lea (%%rax,%s), %%rax\n", tmp);
        fprintf(c->out, "  sar $%d, %%rax\n", ins->aux);
    }
    emit_pow2_store(c, ins);
}

static void emit_binop_into(EmitCtx *c, Instr *ins, int dst_phys, LirWidth w,
                            int off_a, int off_b, const char *op)
{
    const char *dr = reg64_name(dst_phys);
    const char *dr32 = reg32_name(dst_phys);
    int s1 = c->td->scratch1;

    if (off_b != INT_MAX && ins->op != LIR_MUL) {
        load_operand(c, ins->a, dr, w);
        if (w == LIR_W4)
            fprintf(c->out, "  %sl %d(%%rbp), %s\n", op, off_b, dr32);
        else
            fprintf(c->out, "  %sq %d(%%rbp), %s\n", op, off_b, dr);
        if (w == LIR_W4)
            emit_w4_result(c, dst_phys, dr32);
        invalidate_rax(c);
        return;
    }
    if (off_a != INT_MAX && ins->op == LIR_ADD) {
        load_operand(c, ins->b, dr, w);
        if (w == LIR_W4)
            fprintf(c->out, "  addl %d(%%rbp), %s\n", off_a, dr32);
        else
            fprintf(c->out, "  addq %d(%%rbp), %s\n", off_a, dr);
        if (w == LIR_W4)
            emit_w4_result(c, dst_phys, dr32);
        invalidate_rax(c);
        return;
    }
    if (off_b != INT_MAX && ins->op == LIR_MUL) {
        load_operand(c, ins->a, dr, w);
        if (w == LIR_W4)
            fprintf(c->out, "  imull %d(%%rbp), %s\n", off_b, dr32);
        else
            fprintf(c->out, "  imulq %d(%%rbp), %s\n", off_b, dr);
        if (w == LIR_W4)
            emit_w4_result(c, dst_phys, dr32);
        invalidate_rax(c);
        return;
    }

    load_operand(c, ins->a, dr, w);
    if (ins->b.kind == OPND_IMM) {
        if (w == LIR_W4)
            fprintf(c->out, "  %s $%ld, %s\n", op, ins->b.u.imm, dr32);
        else
            fprintf(c->out, "  %s $%ld, %s\n", op, ins->b.u.imm, dr);
    } else {
        load_operand(c, ins->b, reg64_name(s1), w);
        if (w == LIR_W4)
            fprintf(c->out, "  %s %s, %s\n", op, reg32_name(s1), dr32);
        else
            fprintf(c->out, "  %s %s, %s\n", op, reg64_name(s1), dr);
    }
    if (w == LIR_W4)
        emit_w4_result(c, dst_phys, dr32);
    invalidate_rax(c);
}

static void emit_reg_to_stack(EmitCtx *c, int phys, int offset, int bytes)
{
    switch (bytes) {
    case 1:
        fprintf(c->out, "  mov %s, %d(%%rbp)\n", reg8_name(phys), offset);
        return;
    case 2:
        fprintf(c->out, "  mov %s, %d(%%rbp)\n", reg16_name(phys), offset);
        return;
    case 4:
        fprintf(c->out, "  mov %s, %d(%%rbp)\n", reg32_name(phys), offset);
        return;
    case 8:
        fprintf(c->out, "  mov %s, %d(%%rbp)\n", reg64_name(phys), offset);
        return;
    default:
        break;
    }

    /* Record tails can have any width from 1 through 7.  Store them in
     * exact-width chunks rather than overwriting adjacent frame slots. */
    fprintf(c->out, "  mov %s, %%rax\n", reg64_name(phys));
    emit_store_rax_partial(c, "%rbp", offset, bytes);
}

static void emit_record_param_spill(EmitCtx *c, const TargetDesc *td, Param *p)
{
    AbiArgPlan ap;
    Type *ty = type_decay(p->ty);
    int off = (int)fp_disp(c, p->offset);

    abi_arg_plan(ty, &ap);
    emit_reg_to_stack(c, td->arg_regs[p->abi_gpr_start], off,
                      ap.size < 8 ? ap.size : 8);
    if (ap.kind == ABI_ARG_GPR_PAIR) {
        int tail = ap.size - 8;
        emit_reg_to_stack(c, td->arg_regs[p->abi_gpr_start + 1],
                          off + 8, tail < 8 ? tail : 8);
    }
}

static void emit_arg_reg_store(EmitCtx *c, int phys, Type *ty, int offset)
{
    TypeScalarInfo si;
    int w = type_scalar_info(ty, &si) ? si.width : 8;
    int off = (int)fp_disp(c, offset);

    switch (w) {
    case 1:
        fprintf(c->out, "  mov %s, %d(%%rbp)\n", reg8_name(phys), off);
        return;
    case 2:
        fprintf(c->out, "  mov %s, %d(%%rbp)\n", reg16_name(phys), off);
        return;
    case 4:
        fprintf(c->out, "  mov %s, %d(%%rbp)\n", reg32_name(phys), off);
        return;
    default:
        fprintf(c->out, "  mov %s, %d(%%rbp)\n", reg64_name(phys), off);
        return;
    }
}

static void emit_instr(EmitCtx *c, Instr *ins)
{
    const TargetDesc *td = c->td;
    int s0 = td->scratch0;
    int s1 = td->scratch1;

    switch (ins->op) {
    case LIR_MOVI: {
        int dp;
        if (ins->dst != LIR_NO_VREG && dst_phys(c, ins->dst, &dp)) {
            fprintf(c->out, "  mov $%ld, %s\n",
                    ins->a.u.imm, reg64_name(dp));
            invalidate_rax(c);
            return;
        }
        fprintf(c->out, "  mov $%ld, %%rax\n", ins->a.u.imm);
        if (ins->dst != LIR_NO_VREG)
            store_vreg_slot(c, ins->dst, "%rax");
        return;
    }

    case LIR_MOV:
        if (ins->dst == LIR_NO_VREG) {
            if (ins->b.kind == OPND_PHYS) {
                load_operand(c, ins->a, reg64_name(ins->b.u.phys), LIR_W8);
                if (ins->b.u.phys == PHYS_RAX) {
                    if (ins->a.kind == OPND_VREG)
                        c->rax_vreg = ins->a.u.vreg;
                    else
                        c->rax_vreg = -1;
                }
            } else {
                load_operand(c, ins->a, "%rax", LIR_W8);
                invalidate_rax(c);
            }
            return;
        }
        if (ins->a.kind == OPND_VREG && ins->a.u.vreg == ins->dst)
            return;
        {
            int dp;
            if (dst_phys(c, ins->dst, &dp)) {
                load_operand(c, ins->a, reg64_name(dp), LIR_W8);
                invalidate_rax(c);
                return;
            }
        }
        load_operand(c, ins->a, "%rax", LIR_W8);
        store_vreg_slot(c, ins->dst, "%rax");
        return;

    case LIR_LOAD:
        if (ins->a.kind == OPND_MEM && ins->a.u.mem.base == LIR_FP) {
            emit_load_fp_slot(c, ins->a, ins->aux, ins->sgn);
        } else {
            load_mem_addr(c, ins->a, "%r10");
            switch (ins->aux) {
            case 1:
                if (ins->sgn == LIR_SGN_S)
                    fprintf(c->out, "  movsbl (%%r10), %%eax\n");
                else
                    fprintf(c->out, "  movzbl (%%r10), %%eax\n");
                break;
            case 2:
                if (ins->sgn == LIR_SGN_S)
                    fprintf(c->out, "  movswl (%%r10), %%eax\n");
                else
                    fprintf(c->out, "  movzwl (%%r10), %%eax\n");
                break;
            case 4:
                fprintf(c->out, "  movslq (%%r10), %%rax\n");
                break;
            case 8:
                fprintf(c->out, "  mov (%%r10), %%rax\n");
                break;
            default:
                emit_load_rax_partial(c, "%r10", 0, ins->aux);
                break;
            }
        }
        store_vreg_slot(c, ins->dst, "%rax");
        return;

    case LIR_STORE:
        load_operand(c, ins->b, "%rax", ins->w);
        if (ins->a.kind == OPND_MEM && ins->a.u.mem.base == LIR_FP)
            emit_store_fp_slot(c, ins->a, ins->aux);
        else
            emit_store_mem(c, ins->a, ins->w, ins->aux);
        return;

    case LIR_LEA:
        load_mem_addr(c, ins->a, "%rax");
        store_vreg_slot(c, ins->dst, "%rax");
        return;

    case LIR_LEA_SYM:
        fprintf(c->out, "  leaq %s(%%rip), %%rax\n", ins->sym_name);
        store_vreg_slot(c, ins->dst, "%rax");
        return;

    case LIR_ADD:
    case LIR_SUB:
    case LIR_MUL:
    case LIR_AND:
    case LIR_OR:
    case LIR_SHL:
    case LIR_SHR:
    case LIR_SAR: {
        LirWidth w = ins->w;
        int off_b = spilled_vreg_off(c, ins->b);
        int off_a = spilled_vreg_off(c, ins->a);
        const char *op = ins->op == LIR_ADD ? "add" :
                         ins->op == LIR_SUB ? "sub" :
                         ins->op == LIR_MUL ? "imul" :
                         ins->op == LIR_AND ? "and" :
                         ins->op == LIR_OR ? "or" :
                         ins->op == LIR_SHL ? "sal" :
                         ins->op == LIR_SHR ? "shr" : "sar";
        int dp;

        if (ins->dst != LIR_NO_VREG && dst_phys(c, ins->dst, &dp)) {
            emit_binop_into(c, ins, dp, w, off_a, off_b, op);
            return;
        }

        if (off_b != INT_MAX && ins->op != LIR_MUL) {
            load_operand(c, ins->a, reg64_name(s0), w);
            if (w == LIR_W4)
                fprintf(c->out, "  %sl %d(%%rbp), %s\n",
                        op, off_b, reg32_name(s0));
            else
                fprintf(c->out, "  %sq %d(%%rbp), %s\n",
                        op, off_b, reg64_name(s0));
            if (w == LIR_W4)
                fprintf(c->out, "  mov %s, %%rax\n", reg64_name(s0));
            else
                fprintf(c->out, "  mov %s, %%rax\n", reg64_name(s0));
            store_vreg_slot(c, ins->dst, "%rax");
            return;
        }
        if (off_a != INT_MAX && ins->op == LIR_ADD) {
            load_operand(c, ins->b, reg64_name(s0), w);
            if (w == LIR_W4)
                fprintf(c->out, "  addl %d(%%rbp), %s\n", off_a, reg32_name(s0));
            else
                fprintf(c->out, "  addq %d(%%rbp), %s\n", off_a, reg64_name(s0));
            if (w == LIR_W4)
                fprintf(c->out, "  mov %s, %%rax\n", reg64_name(s0));
            else
                fprintf(c->out, "  mov %s, %%rax\n", reg64_name(s0));
            store_vreg_slot(c, ins->dst, "%rax");
            return;
        }
        if (off_b != INT_MAX && ins->op == LIR_MUL) {
            load_operand(c, ins->a, reg64_name(s0), w);
            if (w == LIR_W4)
                fprintf(c->out, "  imull %d(%%rbp), %s\n", off_b, reg32_name(s0));
            else
                fprintf(c->out, "  imulq %d(%%rbp), %s\n", off_b, reg64_name(s0));
            if (w == LIR_W4)
                fprintf(c->out, "  mov %s, %%rax\n", reg64_name(s0));
            else
                fprintf(c->out, "  mov %s, %%rax\n", reg64_name(s0));
            store_vreg_slot(c, ins->dst, "%rax");
            return;
        }

        load_operand(c, ins->a, reg64_name(s0), w);
        if (ins->b.kind == OPND_IMM) {
            if (w == LIR_W4)
                fprintf(c->out, "  %s $%ld, %s\n",
                        op, ins->b.u.imm, reg32_name(s0));
            else
                fprintf(c->out, "  %s $%ld, %s\n",
                        op, ins->b.u.imm, reg64_name(s0));
        } else {
            load_operand(c, ins->b, reg64_name(s1), w);
            if (w == LIR_W4)
                fprintf(c->out, "  %s %s, %s\n",
                        op, reg32_name(s1), reg32_name(s0));
            else
                fprintf(c->out, "  %s %s, %s\n",
                        op, reg64_name(s1), reg64_name(s0));
        }
        if (w == LIR_W4)
            fprintf(c->out, "  movslq %s, %%rax\n", reg32_name(s0));
        else
            fprintf(c->out, "  mov %s, %%rax\n", reg64_name(s0));
        store_vreg_slot(c, ins->dst, "%rax");
        return;
    }

    case LIR_SDIV_POW2:
        emit_sdiv_pow2(c, ins);
        return;
    case LIR_SMOD_POW2:
        emit_smod_pow2(c, ins);
        return;
    case LIR_UDIV_POW2:
        emit_udiv_pow2(c, ins);
        return;
    case LIR_UMOD_POW2:
        emit_umod_pow2(c, ins);
        return;

    case LIR_DIV:
    case LIR_MOD: {
        LirWidth w = ins->w;
        load_operand(c, ins->a, "%rax", w);
        if (ins->sgn == LIR_SGN_U) {
            if (w == LIR_W4)
                fprintf(c->out, "  xor %%edx, %%edx\n");
            else
                fprintf(c->out, "  xor %%rdx, %%rdx\n");
        } else if (w == LIR_W4) {
            fprintf(c->out, "  cdq\n");
        } else {
            fprintf(c->out, "  cqo\n");
        }
        load_operand(c, ins->b, "%rdi", w);
        if (w == LIR_W4)
            fprintf(c->out, "  %s %%edi\n",
                    ins->sgn == LIR_SGN_U ? "div" : "idiv");
        else
            fprintf(c->out, "  %sq %%rdi\n",
                    ins->sgn == LIR_SGN_U ? "div" : "idiv");
        if (ins->op == LIR_MOD) {
            if (w == LIR_W4)
                fprintf(c->out, "  movslq %%edx, %%rax\n");
            else
                fprintf(c->out, "  mov %%rdx, %%rax\n");
        } else if (w == LIR_W4) {
            fprintf(c->out, "  cltq\n");
        }
        store_vreg_slot(c, ins->dst, "%rax");
        return;
    }

    case LIR_NEG: {
        int dp;
        if (ins->dst != LIR_NO_VREG && dst_phys(c, ins->dst, &dp)) {
            load_operand(c, ins->a, reg64_name(dp), ins->w);
            if (ins->w == LIR_W4)
                fprintf(c->out, "  neg %s\n", reg32_name(dp));
            else
                fprintf(c->out, "  neg %s\n", reg64_name(dp));
            if (ins->w == LIR_W4)
                emit_w4_result(c, dp, reg32_name(dp));
            invalidate_rax(c);
            return;
        }
        load_operand(c, ins->a, "%rax", ins->w);
        if (ins->w == LIR_W4)
            fprintf(c->out, "  neg %%eax\n");
        else
            fprintf(c->out, "  neg %%rax\n");
        if (ins->w == LIR_W4)
            fprintf(c->out, "  cltq\n");
        store_vreg_slot(c, ins->dst, "%rax");
        return;
    }

    case LIR_SETCC: {
        emit_cmp(c, ins->a, ins->b, ins->w);
        fprintf(c->out, "  %s %%al\n", setcc_for_sign(ins->cc, ins->sgn));
        fprintf(c->out, "  movzbq %%al, %%rax\n");
        store_vreg_slot(c, ins->dst, "%rax");
        return;
    }

    case LIR_BR: {
        invalidate_rax(c);
        emit_cmp(c, ins->a, ins->b, ins->w);
        fprintf(c->out, "  %s ", jcc_for_sign(ins->cc, ins->sgn));
        emit_label_ref(c, ins->label);
        fprintf(c->out, "\n");
        return;
    }

    case LIR_JMP:
        invalidate_rax(c);
        fprintf(c->out, "  jmp ");
        emit_label_ref(c, ins->label);
        fprintf(c->out, "\n");
        return;

    case LIR_LABEL:
        invalidate_rax(c);
        emit_label_ref(c, ins->label);
        fprintf(c->out, ":\n");
        return;

    case LIR_CONV: {
        load_operand(c, ins->a, "%rax", LIR_W8);
        switch (ins->conv) {
        case CONV_ZEXT8:
            fprintf(c->out, "  movzbl %%al, %%eax\n");
            break;
        case CONV_SEXT8:
            fprintf(c->out, "  movsbl %%al, %%eax\n");
            break;
        case CONV_ZEXT16:
            fprintf(c->out, "  movzwl %%ax, %%eax\n");
            break;
        case CONV_SEXT16:
            fprintf(c->out, "  movswl %%ax, %%eax\n");
            fprintf(c->out, "  cltq\n");
            break;
        case CONV_ZEXT32:
            fprintf(c->out, "  mov %%eax, %%eax\n");
            break;
        case CONV_SEXT32_64:
            fprintf(c->out, "  cltq\n");
            break;
        case CONV_TRUNC_LO32:
            fprintf(c->out, "  movslq %%eax, %%rax\n");
            break;
        }
        store_vreg_slot(c, ins->dst, "%rax");
        return;
    }

    case LIR_CALL: {
        invalidate_rax(c);
        for (int i = ins->call_nreg; i < ins->nargs; i++) {
            load_operand(c, ins->call_args[i], "%rax", LIR_W8);
            fprintf(c->out, "  mov %%rax, %d(%%rsp)\n",
                    8 * (i - ins->call_nreg));
        }
        for (int i = 0; i < ins->call_nreg; i++) {
            int preg = td->arg_regs[i];
            load_operand(c, ins->call_args[i], reg64_name(preg), LIR_W8);
        }
        fprintf(c->out, "  mov $0, %%al\n");
        if (ins->call_indirect) {
            load_operand(c, lir_vreg(ins->call_reg), "%rax", LIR_W8);
            fprintf(c->out, "  call *%%rax\n");
        } else {
            fprintf(c->out, "  call %s\n", ins->call_name);
        }
        return;
    }

    case LIR_RET:
        return;

    case LIR_MEMCPY: {
        int size = ins->aux;

        load_operand(c, ins->a, "%rdi", LIR_W8);
        load_operand(c, ins->b, "%rsi", LIR_W8);
        if (size <= 8) {
            if (size >= 8)
                fprintf(c->out, "  mov (%%rsi), %%rax\n");
            else if (size >= 4)
                fprintf(c->out, "  movslq (%%rsi), %%rax\n");
            else if (size >= 2)
                fprintf(c->out, "  movswq (%%rsi), %%rax\n");
            else
                fprintf(c->out, "  movsbq (%%rsi), %%rax\n");
            if (size >= 8)
                fprintf(c->out, "  mov %%rax, (%%rdi)\n");
            else if (size >= 4)
                fprintf(c->out, "  mov %%eax, (%%rdi)\n");
            else if (size >= 2)
                fprintf(c->out, "  mov %%ax, (%%rdi)\n");
            else
                fprintf(c->out, "  mov %%al, (%%rdi)\n");
        } else {
            fprintf(c->out, "  mov $%d, %%rcx\n", size);
            fprintf(c->out, "  cld\n");
            fprintf(c->out, "  rep movsb\n");
        }
        invalidate_rax(c);
        return;
    }
    }
}

static const int x86_callee_save_order[] = {
    PHYS_RBX, PHYS_R12, PHYS_R13, PHYS_R14, PHYS_R15,
};

static void emit_prologue(EmitCtx *c)
{
    AllocResult *alloc = c->alloc;
    int save_pad = callee_saved_count(alloc) & 1 ? 8 : 0;

    fprintf(c->out, "  push %%rbp\n");
    fprintf(c->out, "  mov %%rsp, %%rbp\n");
    for (int i = 0; i < 5; i++) {
        int r = x86_callee_save_order[i];
        if (alloc->used_callee_saved & (1u << r))
            fprintf(c->out, "  push %s\n", reg64_name(r));
    }
    if (save_pad)
        fprintf(c->out, "  sub $%d, %%rsp\n", save_pad);
    if (alloc->frame_size)
        fprintf(c->out, "  sub $%d, %%rsp\n", alloc->frame_size);
}

static void emit_epilogue(EmitCtx *c)
{
    AllocResult *alloc = c->alloc;
    int save_pad = callee_saved_count(alloc) & 1 ? 8 : 0;

    if (alloc->frame_size)
        fprintf(c->out, "  add $%d, %%rsp\n", alloc->frame_size);
    if (save_pad)
        fprintf(c->out, "  add $%d, %%rsp\n", save_pad);
    for (int i = 4; i >= 0; i--) {
        int r = x86_callee_save_order[i];
        if (alloc->used_callee_saved & (1u << r))
            fprintf(c->out, "  pop %s\n", reg64_name(r));
    }
    fprintf(c->out, "  pop %%rbp\n");
    fprintf(c->out, "  ret\n");
}

void emit_x86_function(LirFn *lf, Function *fn, AllocResult *alloc,
                       FILE *out, const TargetDesc *td)
{
    EmitCtx ctx = {
        .out = out, .lf = lf, .fn = fn, .alloc = alloc, .td = td,
        .rax_vreg = -1,
    };

    fprintf(out, "  .globl %s\n", fn->name);
    fprintf(out, "%s:\n", fn->name);

    emit_prologue(&ctx);

    if (fn->abi_ret_sret)
        fprintf(out, "  mov %%rdi, %ld(%%rbp)\n",
                fp_disp(&ctx, fn->abi_sret_offset));

    for (Param *p = fn->params; p; p = p->next) {
        if (p->abi_gpr_start < 0)
            continue;
        if (lir_home_vreg(lf, p->offset) != LIR_NO_VREG)
            continue;
        if (abi_type_is_record_pass(type_decay(p->ty)))
            emit_record_param_spill(&ctx, td, p);
        else
            emit_arg_reg_store(&ctx, td->arg_regs[p->abi_gpr_start],
                               type_decay(p->ty), p->offset);
    }

    for (int j = 0; j < lf->ninstr; j++) {
        Instr *ins = &lf->instrs[j];
        if (ins->op == LIR_RET)
            continue;
        emit_instr(&ctx, ins);
    }

    emit_epilogue(&ctx);
}
