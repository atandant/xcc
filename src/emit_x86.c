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
    static const char *names[PHYS_XMM0] = {
        "%rax", "%rdx", "%rcx", "%rbx", "%rsi", "%rdi",
        "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15",
    };
    assert(phys >= 0 && phys < PHYS_XMM0);
    return names[phys];
}

static const char *reg32_name(int phys)
{
    static const char *names[PHYS_XMM0] = {
        "%eax", "%edx", "%ecx", "%ebx", "%esi", "%edi",
        "%r8d", "%r9d", "%r10d", "%r11d", "%r12d", "%r13d", "%r14d", "%r15d",
    };
    return names[phys];
}

static const char *reg16_name(int phys)
{
    static const char *names[PHYS_XMM0] = {
        "%ax", "%dx", "%cx", "%bx", "%si", "%di",
        "%r8w", "%r9w", "%r10w", "%r11w", "%r12w", "%r13w", "%r14w", "%r15w",
    };
    return names[phys];
}

static const char *reg8_name(int phys)
{
    static const char *names[PHYS_XMM0] = {
        "%al", "%dl", "%cl", "%bl", "%sil", "%dil",
        "%r8b", "%r9b", "%r10b", "%r11b", "%r12b", "%r13b", "%r14b", "%r15b",
    };
    return names[phys];
}

static const char *x86_reg_name(int phys, int width)
{
    static const char *xmm_names[16] = {
        "%xmm0", "%xmm1", "%xmm2", "%xmm3",
        "%xmm4", "%xmm5", "%xmm6", "%xmm7",
        "%xmm8", "%xmm9", "%xmm10", "%xmm11",
        "%xmm12", "%xmm13", "%xmm14", "%xmm15",
    };

    if (phys >= PHYS_XMM0 && phys < PHYS_COUNT)
        return xmm_names[phys - PHYS_XMM0];
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

static const int x86_xmm_alloc_order[] = {
    PHYS_XMM8, PHYS_XMM9, PHYS_XMM10, PHYS_XMM11,
    PHYS_XMM12, PHYS_XMM13,
    PHYS_XMM0, PHYS_XMM1, PHYS_XMM2, PHYS_XMM3,
    PHYS_XMM4, PHYS_XMM5, PHYS_XMM6, PHYS_XMM7,
};

static const int x86_arg_regs[] = {
    PHYS_RDI, PHYS_RSI, PHYS_RDX, PHYS_RCX, PHYS_R8, PHYS_R9,
};

#define X86_CALLER_SAVED_MASK \
    ((1u << PHYS_RAX) | (1u << PHYS_RCX) | (1u << PHYS_RDX) | \
     (1u << PHYS_RSI) | (1u << PHYS_RDI) | (1u << PHYS_R8) | \
     (1u << PHYS_R9) | (1u << PHYS_R10) | (1u << PHYS_R11) | \
     (1u << PHYS_XMM0) | (1u << PHYS_XMM1) | (1u << PHYS_XMM2) | \
     (1u << PHYS_XMM3) | (1u << PHYS_XMM4) | (1u << PHYS_XMM5) | \
     (1u << PHYS_XMM6) | (1u << PHYS_XMM7) | (1u << PHYS_XMM8) | \
     (1u << PHYS_XMM9) | (1u << PHYS_XMM10) | (1u << PHYS_XMM11) | \
     (1u << PHYS_XMM12) | (1u << PHYS_XMM13) | (1u << PHYS_XMM14) | \
     (1u << PHYS_XMM15))

#define X86_CALLEE_SAVED_MASK \
    ((1u << PHYS_RBX) | (1u << PHYS_R12) | (1u << PHYS_R13) | \
     (1u << PHYS_R14) | (1u << PHYS_R15))

const TargetDesc X86_SYSV = {
    .name = "x86-64-sysv",
    .nalloc = { 9, 14, 0 },
    .alloc_order = { x86_alloc_order, x86_xmm_alloc_order, NULL },
    .reg_name = x86_reg_name_fn,
    .caller_saved_mask = X86_CALLER_SAVED_MASK,
    .callee_saved_mask = X86_CALLEE_SAVED_MASK,
    .arg_regs = x86_arg_regs,
    .nargs_reg = 6,
    .ret_reg = PHYS_RAX,
    .div_num_reg = PHYS_RAX,
    .div_rem_reg = PHYS_RDX,
    .shift_count_reg = PHYS_RCX,
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

#define X86_MEM_NO_REG (-1)
#define X86_MEM_FRAME  (-2)

typedef struct {
    int base;
    int index;
    int scale;
    long disp;
} X86MemRef;

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

static void store_vreg_slot(EmitCtx *c, int v, const char *reg)
{
    store_vreg_value(c, v, reg);
}

static const char *xmm_name(int phys)
{
    return x86_reg_name(phys, 8);
}

static const char *fp_mov(LirFloatWidth fpw)
{
    return fpw == LIR_FP_F32 ? "movss" : "movsd";
}

static void materialize_float(EmitCtx *c, int v, int target,
                              LirFloatWidth fpw)
{
    int phys = vreg_phys(c, v);

    assert(target >= PHYS_XMM0);
    if (phys >= 0) {
        assert(phys >= PHYS_XMM0);
        if (phys != target)
            fprintf(c->out, "  movaps %s, %s\n", xmm_name(phys),
                    xmm_name(target));
    } else {
        fprintf(c->out, "  %s %d(%%rbp), %s\n", fp_mov(fpw),
                vreg_off(c, v), xmm_name(target));
    }
}

static void store_float(EmitCtx *c, int v, int source, LirFloatWidth fpw)
{
    int phys = vreg_phys(c, v);

    assert(source >= PHYS_XMM0);
    if (phys >= 0) {
        assert(phys >= PHYS_XMM0);
        if (phys != source)
            fprintf(c->out, "  movaps %s, %s\n", xmm_name(source),
                    xmm_name(phys));
    } else {
        fprintf(c->out, "  %s %s, %d(%%rbp)\n", fp_mov(fpw),
                xmm_name(source), vreg_off(c, v));
    }
}

static void load_float_operand(EmitCtx *c, Operand op, int target,
                               LirFloatWidth fpw)
{
    if (op.kind == OPND_VREG) {
        materialize_float(c, op.u.vreg, target, fpw);
        return;
    }
    if (op.kind == OPND_PHYS) {
        assert(op.u.phys >= PHYS_XMM0);
        if (op.u.phys != target)
            fprintf(c->out, "  movaps %s, %s\n", xmm_name(op.u.phys),
                    xmm_name(target));
        return;
    }
    assert(0 && "invalid floating operand");
}

static int float_operand_phys(EmitCtx *c, Operand op)
{
    if (op.kind == OPND_PHYS)
        return op.u.phys;
    if (op.kind == OPND_VREG)
        return vreg_phys(c, op.u.vreg);
    return REG_NONE;
}

static void emit_float_operand_ref(EmitCtx *c, Operand op)
{
    int phys = float_operand_phys(c, op);

    if (phys >= 0) {
        assert(phys >= PHYS_XMM0);
        fprintf(c->out, "%s", xmm_name(phys));
        return;
    }
    assert(op.kind == OPND_VREG);
    fprintf(c->out, "%d(%%rbp)", vreg_off(c, op.u.vreg));
}

static int f80_operand_off(EmitCtx *c, Operand op)
{
    assert(op.kind == OPND_VREG);
    assert(lir_vreg_type(c->lf, op.u.vreg) == LIR_TYPE_F80);
    assert(vreg_phys(c, op.u.vreg) < 0);
    return vreg_off(c, op.u.vreg);
}

static void emit_float_cmp(EmitCtx *c, Operand a, Operand b,
                           LirFloatWidth fpw)
{
    if (fpw == LIR_FP_F80) {
        fprintf(c->out,
                "  fldt %d(%%rbp)\n"
                "  fldt %d(%%rbp)\n"
                "  fucomip %%st(1), %%st\n"
                "  fstp %%st(0)\n",
                f80_operand_off(c, b), f80_operand_off(c, a));
        return;
    }
    load_float_operand(c, a, PHYS_XMM15, fpw);
    fprintf(c->out, "  ucomis%c ", fpw == LIR_FP_F32 ? 's' : 'd');
    emit_float_operand_ref(c, b);
    fprintf(c->out, ", %s\n", xmm_name(PHYS_XMM15));
}

static void emit_float_setcc(EmitCtx *c, LirCond cc)
{
    const char *tmp = reg8_name(c->td->scratch0);

    switch (cc) {
    case CC_EQ:
        fprintf(c->out, "  sete %%al\n  setnp %s\n  and %s, %%al\n",
                tmp, tmp);
        break;
    case CC_NE:
        fprintf(c->out, "  setne %%al\n  setp %s\n  or %s, %%al\n",
                tmp, tmp);
        break;
    case CC_LT:
        fprintf(c->out, "  setb %%al\n  setnp %s\n  and %s, %%al\n",
                tmp, tmp);
        break;
    case CC_LE:
        fprintf(c->out, "  setbe %%al\n  setnp %s\n  and %s, %%al\n",
                tmp, tmp);
        break;
    case CC_GT:
        fprintf(c->out, "  seta %%al\n");
        break;
    case CC_GE:
        fprintf(c->out, "  setae %%al\n");
        break;
    }
}

static X86MemRef prepare_mem_ref(EmitCtx *c, Operand mem)
{
    X86MemRef ref = {
        .index = X86_MEM_NO_REG,
        .scale = mem.u.mem.scale,
        .disp = mem.u.mem.disp,
    };

    assert(mem.kind == OPND_MEM);
    if (mem.u.mem.base == LIR_FP) {
        ref.base = X86_MEM_FRAME;
        ref.disp = fp_disp(c, ref.disp);
    } else {
        ref.base = vreg_phys(c, mem.u.mem.base);
        if (ref.base < 0) {
            ref.base = c->td->scratch0;
            materialize_vreg(c, mem.u.mem.base, reg64_name(ref.base));
        }
    }

    if (mem.u.mem.index == LIR_NO_IDX)
        return ref;
    if (mem.u.mem.base != LIR_FP &&
        mem.u.mem.index == mem.u.mem.base) {
        ref.index = ref.base;
        return ref;
    }
    ref.index = vreg_phys(c, mem.u.mem.index);
    if (ref.index < 0) {
        ref.index = c->td->scratch1;
        materialize_vreg(c, mem.u.mem.index, reg64_name(ref.index));
    }
    return ref;
}

static void emit_mem_ref(EmitCtx *c, X86MemRef ref)
{
    const char *base = ref.base == X86_MEM_FRAME
                     ? "%rbp" : reg64_name(ref.base);

    fprintf(c->out, "%ld(%s", ref.disp, base);
    if (ref.index != X86_MEM_NO_REG)
        fprintf(c->out, ",%s,%d", reg64_name(ref.index), ref.scale);
    fputc(')', c->out);
}

static void emit_mem_ref_at(EmitCtx *c, X86MemRef ref, long delta)
{
    ref.disp += delta;
    emit_mem_ref(c, ref);
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
    case OPND_MEM: {
        X86MemRef ref = prepare_mem_ref(c, op);

        switch (w) {
        case LIR_W4:
            fprintf(c->out, "  movslq ");
            emit_mem_ref(c, ref);
            fprintf(c->out, ", %s\n", reg);
            return;
        case LIR_W8:
            fprintf(c->out, "  mov ");
            emit_mem_ref(c, ref);
            fprintf(c->out, ", %s\n", reg);
            return;
        }
        return;
    }
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

static int operand_depends_on_phys(EmitCtx *c, Operand operand, int phys)
{
    if (operand.kind == OPND_PHYS)
        return operand.u.phys == phys;
    if (operand.kind == OPND_VREG)
        return vreg_phys(c, operand.u.vreg) == phys;
    if (operand.kind != OPND_MEM)
        return 0;
    if (operand.u.mem.base != LIR_FP &&
        vreg_phys(c, operand.u.mem.base) == phys)
        return 1;
    return operand.u.mem.index != LIR_NO_IDX &&
           vreg_phys(c, operand.u.mem.index) == phys;
}

static int operand_is_phys(EmitCtx *c, Operand operand, int phys)
{
    if (operand.kind == OPND_PHYS)
        return operand.u.phys == phys;
    return operand.kind == OPND_VREG &&
           vreg_phys(c, operand.u.vreg) == phys;
}

static void emit_call_register_args(EmitCtx *c, const Instr *ins)
{
    Operand args[6];
    unsigned pending = 0;

    for (int i = 0; i < ins->call_ngpr; i++) {
        args[i] = ins->call_args[i];
        if (!operand_is_phys(c, args[i], c->td->arg_regs[i]))
            pending |= 1u << i;
    }

    while (pending) {
        int progress = 0;

        for (int i = 0; i < ins->call_ngpr; i++) {
            int dst;
            int needed = 0;

            if (!(pending & (1u << i)))
                continue;
            dst = c->td->arg_regs[i];
            for (int j = 0; j < ins->call_ngpr; j++) {
                if (j != i && (pending & (1u << j)) &&
                    operand_depends_on_phys(c, args[j], dst)) {
                    needed = 1;
                    break;
                }
            }
            if (needed)
                continue;
            load_operand(c, args[i], reg64_name(dst), LIR_W8);
            pending &= ~(1u << i);
            progress = 1;
        }

        if (!progress) {
            int first = 0;
            while (!(pending & (1u << first)))
                first++;
            load_operand(c, args[first], reg64_name(c->td->scratch0), LIR_W8);
            args[first] = lir_phys(c->td->scratch0);
        }
    }
}

static LirFloatWidth call_arg_float_width(EmitCtx *c, const Instr *ins,
                                          int index)
{
    LirType type;

    if (ins->call_arg_types)
        type = ins->call_arg_types[index];
    else {
        Operand arg = ins->call_args[index];
        assert(arg.kind == OPND_VREG);
        type = lir_vreg_type(c->lf, arg.u.vreg);
    }
    assert(type == LIR_TYPE_F32 || type == LIR_TYPE_F64);
    return type == LIR_TYPE_F32 ? LIR_FP_F32 : LIR_FP_F64;
}

static void emit_store_rax_partial(EmitCtx *c, X86MemRef ref, int bytes)
{
    int done = 0;

    if (bytes >= 4) {
        fprintf(c->out, "  mov %%eax, ");
        emit_mem_ref_at(c, ref, done);
        fputc('\n', c->out);
        fprintf(c->out, "  shr $32, %%rax\n");
        done += 4;
        bytes -= 4;
    }
    if (bytes >= 2) {
        fprintf(c->out, "  mov %%ax, ");
        emit_mem_ref_at(c, ref, done);
        fputc('\n', c->out);
        fprintf(c->out, "  shr $16, %%rax\n");
        done += 2;
        bytes -= 2;
    }
    if (bytes == 1) {
        fprintf(c->out, "  mov %%al, ");
        emit_mem_ref_at(c, ref, done);
        fputc('\n', c->out);
    }
}

static void emit_load_rax_partial(EmitCtx *c, X86MemRef ref, int bytes)
{
    int done = 0;
    int tmp = c->td->scratch1;

    /* The tail loads use scratch1 while assembling the value in %rax.  If a
       spilled index already occupies it, first collapse the address into
       scratch0 so later chunks cannot destroy their own index. */
    if (ref.index == tmp) {
        fprintf(c->out, "  lea ");
        emit_mem_ref(c, ref);
        fprintf(c->out, ", %s\n", reg64_name(c->td->scratch0));
        ref.base = c->td->scratch0;
        ref.index = X86_MEM_NO_REG;
        ref.scale = 1;
        ref.disp = 0;
    }

    fprintf(c->out, "  xor %%eax, %%eax\n");
    if (bytes >= 4) {
        fprintf(c->out, "  mov ");
        emit_mem_ref(c, ref);
        fprintf(c->out, ", %%eax\n");
        done = 4;
    } else if (bytes >= 2) {
        fprintf(c->out, "  movzwl ");
        emit_mem_ref(c, ref);
        fprintf(c->out, ", %%eax\n");
        done = 2;
    } else if (bytes == 1) {
        fprintf(c->out, "  movzbl ");
        emit_mem_ref(c, ref);
        fprintf(c->out, ", %%eax\n");
        return;
    }
    if (bytes - done >= 2) {
        fprintf(c->out, "  movzwl ");
        emit_mem_ref_at(c, ref, done);
        fprintf(c->out, ", %s\n", reg32_name(tmp));
        fprintf(c->out, "  shl $%d, %s\n", done * 8, reg64_name(tmp));
        fprintf(c->out, "  or %s, %%rax\n", reg64_name(tmp));
        done += 2;
    }
    if (bytes - done == 1) {
        fprintf(c->out, "  movzbl ");
        emit_mem_ref_at(c, ref, done);
        fprintf(c->out, ", %s\n", reg32_name(tmp));
        fprintf(c->out, "  shl $%d, %s\n", done * 8, reg64_name(tmp));
        fprintf(c->out, "  or %s, %%rax\n", reg64_name(tmp));
    }
}

static int emit_load_scalar_to_phys(EmitCtx *c, X86MemRef ref, int bytes,
                                    LirSign sgn, int phys)
{
    switch (bytes) {
    case 1:
        fprintf(c->out, sgn == LIR_SGN_S ? "  movsbl " : "  movzbl ");
        emit_mem_ref(c, ref);
        fprintf(c->out, ", %s\n", reg32_name(phys));
        return 1;
    case 2:
        fprintf(c->out, sgn == LIR_SGN_S ? "  movswl " : "  movzwl ");
        emit_mem_ref(c, ref);
        fprintf(c->out, ", %s\n", reg32_name(phys));
        return 1;
    case 4:
        fprintf(c->out, "  movslq ");
        emit_mem_ref(c, ref);
        fprintf(c->out, ", %s\n", reg64_name(phys));
        return 1;
    case 8:
        fprintf(c->out, "  mov ");
        emit_mem_ref(c, ref);
        fprintf(c->out, ", %s\n", reg64_name(phys));
        return 1;
    default:
        return 0;
    }
}

static int emit_store_scalar_from_rax(EmitCtx *c, X86MemRef ref, int bytes)
{
    const char *src;

    switch (bytes) {
    case 1: src = "%al"; break;
    case 2: src = "%ax"; break;
    case 4: src = "%eax"; break;
    case 8: src = "%rax"; break;
    default: return 0;
    }
    fprintf(c->out, "  mov %s, ", src);
    emit_mem_ref(c, ref);
    fputc('\n', c->out);
    return 1;
}

#if 0 /* Emitter support for LIR `add/sub [fp+off], $imm` (see lower.c). */
static int is_fp_mem_imm_update(const Instr *ins)
{
    return ins->dst == LIR_NO_VREG &&
           ins->a.kind == OPND_MEM &&
           ins->a.u.mem.base == LIR_FP &&
           ins->a.u.mem.index == LIR_NO_IDX &&
           ins->b.kind == OPND_IMM;
}

static void emit_fp_mem_imm_binop(EmitCtx *c, Instr *ins)
{
    long off = fp_disp(c, ins->a.u.mem.disp);
    int bytes = ins->aux > 0 ? ins->aux : 4;
    const char *op = ins->op == LIR_ADD ? "add" :
                     ins->op == LIR_SUB ? "sub" : "?";

    switch (bytes) {
    case 1:
        fprintf(c->out, "  %sb $%ld, %ld(%%rbp)\n", op, ins->b.u.imm, off);
        break;
    case 2:
        fprintf(c->out, "  %sw $%ld, %ld(%%rbp)\n", op, ins->b.u.imm, off);
        break;
    case 4:
        fprintf(c->out, "  %sl $%ld, %ld(%%rbp)\n", op, ins->b.u.imm, off);
        break;
    case 8:
        fprintf(c->out, "  %sq $%ld, %ld(%%rbp)\n", op, ins->b.u.imm, off);
        break;
    default:
        assert(0 && "unexpected fp mem binop width");
    }
    invalidate_rax(c);
}
#endif

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

static void emit_shift(EmitCtx *c, Instr *ins)
{
    const char *op = ins->op == LIR_SHL ? "sal" :
                     ins->op == LIR_SHR ? "shr" : "sar";
    int result = c->td->scratch0;
    int dp;
    int direct = 0;

    /* A variable shift needs %cl for its count, so it cannot also build its
       result in %rcx.  Otherwise compute directly in the allocated
       destination instead of copying the result out of scratch. */
    if (dst_phys(c, ins->dst, &dp) &&
        (ins->b.kind == OPND_IMM || dp != c->td->shift_count_reg)) {
        result = dp;
        direct = 1;
    }

    load_operand(c, ins->a, reg64_name(result), ins->w);
    if (ins->b.kind == OPND_IMM) {
        fprintf(c->out, "  %s $%ld, %s\n", op, ins->b.u.imm,
                ins->w == LIR_W4 ? reg32_name(result) : reg64_name(result));
    } else {
        load_operand(c, ins->b, reg64_name(c->td->shift_count_reg), LIR_W8);
        fprintf(c->out, "  %s %%cl, %s\n", op,
                ins->w == LIR_W4 ? reg32_name(result) : reg64_name(result));
    }
    if (ins->w == LIR_W4)
        fprintf(c->out, "  movslq %s, %s\n",
                reg32_name(result), reg64_name(result));
    if (!direct)
        store_vreg_slot(c, ins->dst, reg64_name(result));
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
    emit_store_rax_partial(c, (X86MemRef){
        .base = X86_MEM_FRAME, .index = X86_MEM_NO_REG,
        .scale = 1, .disp = offset,
    }, bytes);
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

static int conv_integer_to_f80(EmitCtx *c, const Instr *ins, int s0, int s1)
{
    int dst_off = vreg_off(c, ins->dst);
    const char *r0 = reg64_name(s0);
    const char *r1 = reg64_name(s1);

    if (ins->conv != CONV_SI32_F80 && ins->conv != CONV_SI64_F80 &&
        ins->conv != CONV_UI32_F80 && ins->conv != CONV_UI64_F80)
        return 0;

    if (ins->conv == CONV_SI32_F80) {
        load_operand(c, ins->a, r0, LIR_W4);
        fprintf(c->out, "  mov %s, %d(%%rbp)\n  fildl %d(%%rbp)\n",
                reg32_name(s0), dst_off, dst_off);
    } else if (ins->conv == CONV_SI64_F80 ||
               ins->conv == CONV_UI32_F80) {
        load_operand(c, ins->a, r0, LIR_W8);
        fprintf(c->out, "  mov %s, %d(%%rbp)\n  fildq %d(%%rbp)\n",
                r0, dst_off, dst_off);
    } else {
        /* Split uint64 into its low 63 bits plus an exact 2^63.  Unlike the
           usual halve-and-double SSE sequence, this preserves odd integers
           because x87 extended precision has a full 64-bit significand. */
        load_operand(c, ins->a, r0, LIR_W8);
        fprintf(c->out,
                "  mov %s, %s\n"
                "  btr $63, %s\n"
                "  mov %s, %d(%%rbp)\n"
                "  fildq %d(%%rbp)\n"
                "  test %s, %s\n"
                "  jns 1f\n"
                "  movabs $-9223372036854775808, %s\n"
                "  mov %s, %d(%%rbp)\n"
                "  fildq %d(%%rbp)\n"
                "  fchs\n"
                "  faddp\n"
                "1:\n",
                r0, r1, r1, r1, dst_off, dst_off, r0, r0, r1, r1,
                dst_off, dst_off);
    }
    fprintf(c->out, "  fstpt %d(%%rbp)\n", dst_off);
    return 1;
}

static int conv_f80_to_integer(EmitCtx *c, const Instr *ins, int s0, int s1)
{
    const char *r0 = reg64_name(s0);
    const char *r1 = reg64_name(s1);
    int source_off;

    if (ins->conv != CONV_F80_SI32 && ins->conv != CONV_F80_SI64 &&
        ins->conv != CONV_F80_UI32 && ins->conv != CONV_F80_UI64)
        return 0;
    source_off = f80_operand_off(c, ins->a);

    /* C floating-to-integer conversion truncates toward zero.  Use a temporary
       x87 control word instead of requiring SSE3's FISTTP, then restore the
       caller's control word before exposing the result. */
    fprintf(c->out,
            "  sub $32, %%rsp\n"
            "  fnstcw 28(%%rsp)\n"
            "  movzwl 28(%%rsp), %s\n"
            "  or $3072, %s\n"
            "  movw %s, 30(%%rsp)\n"
            "  fldcw 30(%%rsp)\n",
            reg32_name(s0), reg32_name(s0), reg16_name(s0));

    if (ins->conv == CONV_F80_UI64) {
        /* Values at or above 2^63 are reduced by exactly 2^63, converted as
           signed, then have the high integer bit restored. */
        fprintf(c->out,
                "  movabs $-9223372036854775808, %s\n"
                "  mov %s, 16(%%rsp)\n"
                "  fildq 16(%%rsp)\n"
                "  fchs\n"
                "  fstpt (%%rsp)\n"
                "  fldt (%%rsp)\n"
                "  fldt %d(%%rbp)\n"
                "  fucomip %%st(1), %%st\n"
                "  fstp %%st(0)\n"
                "  jb 1f\n"
                "  fldt (%%rsp)\n"
                "  fldt %d(%%rbp)\n"
                "  fsubp\n"
                "  fistpq 16(%%rsp)\n"
                "  mov 16(%%rsp), %s\n"
                "  movabs $-9223372036854775808, %s\n"
                "  xor %s, %s\n"
                "  jmp 2f\n"
                "1:\n"
                "  fldt %d(%%rbp)\n"
                "  fistpq 16(%%rsp)\n"
                "  mov 16(%%rsp), %s\n"
                "2:\n",
                r0, r0, source_off, source_off, r0, r1, r1, r0,
                source_off, r0);
    } else {
        int signed32 = ins->conv == CONV_F80_SI32;
        char suffix = signed32 ? 'l' : 'q';

        fprintf(c->out,
                "  fldt %d(%%rbp)\n"
                "  fistp%c 16(%%rsp)\n"
                "  mov%s 16(%%rsp), %s\n",
                source_off, suffix,
                suffix == 'l' ? "l" : "", suffix == 'l'
                    ? reg32_name(s0) : r0);
        if (signed32)
            fprintf(c->out, "  movslq %s, %s\n", reg32_name(s0), r0);
    }
    fprintf(c->out, "  fldcw 28(%%rsp)\n  add $32, %%rsp\n");
    store_vreg_slot(c, ins->dst, r0);
    return 1;
}

static void emit_instr(EmitCtx *c, Instr *ins)
{
    const TargetDesc *td = c->td;
    int s0 = td->scratch0;
    int s1 = td->scratch1;

    switch (ins->op) {
    case LIR_FMOVI: {
        if (ins->fpw == LIR_FP_F80) {
            unsigned char bytes[sizeof(long double)];
            unsigned long long lo = 0;
            unsigned short hi = 0;
            int off = vreg_off(c, ins->dst);

            memcpy(bytes, &ins->fimm, sizeof(bytes));
            memcpy(&lo, bytes, 8);
            memcpy(&hi, bytes + 8, 2);
            fprintf(c->out,
                    "  movabs $0x%016llx, %s\n"
                    "  mov %s, %d(%%rbp)\n"
                    "  movw $%u, %d(%%rbp)\n"
                    "  movl $0, %d(%%rbp)\n"
                    "  movw $0, %d(%%rbp)\n",
                    lo, reg64_name(s0), reg64_name(s0), off,
                    (unsigned)hi, off + 8, off + 10, off + 14);
            return;
        }
        int target = vreg_phys(c, ins->dst);
        if (target < 0)
            target = PHYS_XMM15;
        if (ins->fpw == LIR_FP_F32) {
            fprintf(c->out, "  mov $%ld, %s\n  movd %s, %s\n",
                    ins->a.u.imm, reg32_name(s0), reg32_name(s0),
                    xmm_name(target));
        } else {
            fprintf(c->out, "  movabs $%ld, %s\n  movq %s, %s\n",
                    ins->a.u.imm, reg64_name(s0), reg64_name(s0),
                    xmm_name(target));
        }
        store_float(c, ins->dst, target, ins->fpw);
        return;
    }

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
        if (ins->dst >= 0 && lir_vreg_type(c->lf, ins->dst) == LIR_TYPE_F80) {
            if (ins->a.kind == OPND_VREG && ins->a.u.vreg == ins->dst)
                return;
            fprintf(c->out, "  fldt %d(%%rbp)\n  fstpt %d(%%rbp)\n",
                    f80_operand_off(c, ins->a), vreg_off(c, ins->dst));
            return;
        }
        if (ins->dst >= 0 && lir_vreg_class(c->lf, ins->dst) == REG_CLASS_XMM) {
            int target = vreg_phys(c, ins->dst);
            LirFloatWidth fpw = ins->fpw == LIR_FP_NONE ? LIR_FP_F64 : ins->fpw;
            if (target < 0)
                target = PHYS_XMM15;
            load_float_operand(c, ins->a, target, fpw);
            store_float(c, ins->dst, target, fpw);
            return;
        }
        if (ins->dst == LIR_NO_VREG && ins->b.kind == OPND_PHYS &&
            ins->b.u.phys >= PHYS_XMM0) {
            load_float_operand(c, ins->a, ins->b.u.phys,
                               ins->fpw == LIR_FP_NONE ? LIR_FP_F64 : ins->fpw);
            return;
        }
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
                if (operand_is_phys(c, ins->a, dp))
                    return;
                load_operand(c, ins->a, reg64_name(dp), LIR_W8);
                invalidate_rax(c);
                return;
            }
        }
        load_operand(c, ins->a, "%rax", LIR_W8);
        store_vreg_slot(c, ins->dst, "%rax");
        return;

    case LIR_LOAD:
    {
        int dp;
        int target = PHYS_RAX;
        int scalar = ins->aux == 1 || ins->aux == 2 ||
                     ins->aux == 4 || ins->aux == 8;

        if (lir_vreg_type(c->lf, ins->dst) == LIR_TYPE_F80) {
            X86MemRef ref = prepare_mem_ref(c, ins->a);
            fprintf(c->out, "  fldt ");
            emit_mem_ref(c, ref);
            fprintf(c->out, "\n  fstpt %d(%%rbp)\n", vreg_off(c, ins->dst));
            return;
        }
        if (lir_vreg_class(c->lf, ins->dst) == REG_CLASS_XMM) {
            X86MemRef ref = prepare_mem_ref(c, ins->a);
            int fp_target = vreg_phys(c, ins->dst);
            if (fp_target < 0)
                fp_target = PHYS_XMM15;
            fprintf(c->out, "  %s ", fp_mov(ins->fpw));
            emit_mem_ref(c, ref);
            fprintf(c->out, ", %s\n", xmm_name(fp_target));
            store_float(c, ins->dst, fp_target, ins->fpw);
            return;
        }
        if (scalar) {
            X86MemRef ref = prepare_mem_ref(c, ins->a);

            if (dst_phys(c, ins->dst, &dp))
                target = dp;
            if (emit_load_scalar_to_phys(c, ref, ins->aux, ins->sgn,
                                         target)) {
                if (target == PHYS_RAX)
                    store_vreg_slot(c, ins->dst, "%rax");
                return;
            }
        }
        emit_load_rax_partial(c, prepare_mem_ref(c, ins->a), ins->aux);
        store_vreg_slot(c, ins->dst, "%rax");
        return;
    }

    case LIR_STORE:
    {
        X86MemRef ref;
        int scalar = ins->aux == 1 || ins->aux == 2 ||
                     ins->aux == 4 || ins->aux == 8;

        if (ins->b.kind == OPND_VREG &&
            lir_vreg_type(c->lf, ins->b.u.vreg) == LIR_TYPE_F80) {
            fprintf(c->out, "  fldt %d(%%rbp)\n",
                    f80_operand_off(c, ins->b));
            ref = prepare_mem_ref(c, ins->a);
            fprintf(c->out, "  fstpt ");
            emit_mem_ref(c, ref);
            fprintf(c->out, "\n");
            return;
        }
        if (ins->b.kind == OPND_VREG &&
            lir_vreg_class(c->lf, ins->b.u.vreg) == REG_CLASS_XMM) {
            ref = prepare_mem_ref(c, ins->a);
            if (vreg_phys(c, ins->b.u.vreg) < 0)
                load_float_operand(c, ins->b, PHYS_XMM15, ins->fpw);
            fprintf(c->out, "  %s ", fp_mov(ins->fpw));
            if (vreg_phys(c, ins->b.u.vreg) < 0)
                fprintf(c->out, "%s", xmm_name(PHYS_XMM15));
            else
                emit_float_operand_ref(c, ins->b);
            fprintf(c->out, ", ");
            emit_mem_ref(c, ref);
            fprintf(c->out, "\n");
            return;
        }
        load_operand(c, ins->b, "%rax", ins->w);
        ref = prepare_mem_ref(c, ins->a);
        if (scalar)
            emit_store_scalar_from_rax(c, ref, ins->aux);
        else
            emit_store_rax_partial(c, ref, ins->aux);
        return;
    }

    case LIR_LEA:
    {
        int dp;
        int target = PHYS_RAX;
        X86MemRef ref = prepare_mem_ref(c, ins->a);

        if (dst_phys(c, ins->dst, &dp))
            target = dp;
        /* Keep an explicit LIR LEA as an explicit machine instruction.
           Folding it into a later load/store requires use-def and alias
           reasoning and belongs in instruction selection, not formatting. */
        fprintf(c->out, "  lea ");
        emit_mem_ref(c, ref);
        fprintf(c->out, ", %s\n", reg64_name(target));
        if (target == PHYS_RAX)
            store_vreg_slot(c, ins->dst, "%rax");
        return;
    }

    case LIR_LEA_SYM:
    {
        int dp;

        if (dst_phys(c, ins->dst, &dp)) {
            fprintf(c->out, "  leaq %s(%%rip), %s\n",
                    ins->sym_name, reg64_name(dp));
            return;
        }
        fprintf(c->out, "  leaq %s(%%rip), %%rax\n", ins->sym_name);
        store_vreg_slot(c, ins->dst, "%rax");
        return;
    }

    case LIR_ADD:
    case LIR_SUB:
    case LIR_MUL:
    case LIR_AND:
    case LIR_XOR:
    case LIR_OR:
    {
        LirWidth w = ins->w;
        int off_b = spilled_vreg_off(c, ins->b);
        int off_a = spilled_vreg_off(c, ins->a);
        const char *op = ins->op == LIR_ADD ? "add" :
                         ins->op == LIR_SUB ? "sub" :
                         ins->op == LIR_MUL ? "imul" :
                         ins->op == LIR_AND ? "and" :
                         ins->op == LIR_XOR ? "xor" :
                         ins->op == LIR_OR ? "or" :
                         "?";
        int dp;

#if 0 /* see emit_fp_mem_imm_binop in emit_x86.c */
        if (is_fp_mem_imm_update(ins) &&
            (ins->op == LIR_ADD || ins->op == LIR_SUB)) {
            emit_fp_mem_imm_binop(c, ins);
            return;
        }
#endif

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

    case LIR_FADD:
    case LIR_FSUB:
    case LIR_FMUL:
    case LIR_FDIV: {
        const char *op = ins->op == LIR_FADD ? "add" :
                         ins->op == LIR_FSUB ? "sub" :
                         ins->op == LIR_FMUL ? "mul" : "div";
        if (ins->fpw == LIR_FP_F80) {
            fprintf(c->out,
                    "  fldt %d(%%rbp)\n"
                    "  fldt %d(%%rbp)\n"
                    "  f%s%sp\n"
                    "  fstpt %d(%%rbp)\n",
                    f80_operand_off(c, ins->a), f80_operand_off(c, ins->b),
                    op, (ins->op == LIR_FSUB || ins->op == LIR_FDIV) ? "r" : "",
                    vreg_off(c, ins->dst));
            return;
        }
        int target = vreg_phys(c, ins->dst);
        int lhs_phys = float_operand_phys(c, ins->a);
        int rhs_phys = float_operand_phys(c, ins->b);
        int saved_rhs = 0;
        if (target < 0)
            target = PHYS_XMM15;
        if (rhs_phys == target && lhs_phys != target) {
            load_float_operand(c, ins->b, PHYS_XMM14, ins->fpw);
            saved_rhs = 1;
        }
        load_float_operand(c, ins->a, target, ins->fpw);
        fprintf(c->out, "  %s%s ", op,
                ins->fpw == LIR_FP_F32 ? "ss" : "sd");
        if (saved_rhs)
            fprintf(c->out, "%s", xmm_name(PHYS_XMM14));
        else
            emit_float_operand_ref(c, ins->b);
        fprintf(c->out, ", %s\n", xmm_name(target));
        store_float(c, ins->dst, target, ins->fpw);
        return;
    }

    case LIR_FNEG: {
        if (ins->fpw == LIR_FP_F80) {
            fprintf(c->out, "  fldt %d(%%rbp)\n  fchs\n  fstpt %d(%%rbp)\n",
                    f80_operand_off(c, ins->a), vreg_off(c, ins->dst));
            return;
        }
        int target = vreg_phys(c, ins->dst);
        if (target < 0)
            target = PHYS_XMM15;
        load_float_operand(c, ins->a, target, ins->fpw);
        if (ins->fpw == LIR_FP_F32) {
            fprintf(c->out, "  mov $2147483648, %s\n  movd %s, %s\n  xorps %s, %s\n",
                    reg32_name(s0), reg32_name(s0), xmm_name(PHYS_XMM14),
                    xmm_name(PHYS_XMM14), xmm_name(target));
        } else {
            fprintf(c->out, "  movabs $-9223372036854775808, %s\n  movq %s, %s\n  xorpd %s, %s\n",
                    reg64_name(s0), reg64_name(s0), xmm_name(PHYS_XMM14),
                    xmm_name(PHYS_XMM14), xmm_name(target));
        }
        store_float(c, ins->dst, target, ins->fpw);
        return;
    }

    case LIR_FSETCC:
        emit_float_cmp(c, ins->a, ins->b, ins->fpw);
        emit_float_setcc(c, ins->cc);
        fprintf(c->out, "  movzbq %%al, %%rax\n");
        store_vreg_slot(c, ins->dst, "%rax");
        return;

    case LIR_FRET:
        fprintf(c->out, "  fldt %d(%%rbp)\n", f80_operand_off(c, ins->a));
        return;

    case LIR_SHL:
    case LIR_SHR:
    case LIR_SAR:
        emit_shift(c, ins);
        return;

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
        int divisor = c->td->scratch0;

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
        load_operand(c, ins->b, reg64_name(divisor), w);
        if (w == LIR_W4)
            fprintf(c->out, "  %s %s\n",
                    ins->sgn == LIR_SGN_U ? "div" : "idiv",
                    reg32_name(divisor));
        else
            fprintf(c->out, "  %sq %s\n",
                    ins->sgn == LIR_SGN_U ? "div" : "idiv",
                    reg64_name(divisor));
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
        if (ins->fpw != LIR_FP_NONE) {
            emit_float_cmp(c, ins->a, ins->b, ins->fpw);
            emit_float_setcc(c, ins->cc);
            fprintf(c->out, "  test %%al, %%al\n  jne ");
            emit_label_ref(c, ins->label);
            fprintf(c->out, "\n");
            return;
        }
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
        if (ins->conv >= CONV_SI32_F32) {
            if (conv_integer_to_f80(c, ins, s0, s1) ||
                conv_f80_to_integer(c, ins, s0, s1))
                return;
            if (ins->conv == CONV_F32_F80 || ins->conv == CONV_F64_F80) {
                LirFloatWidth srcw = ins->conv == CONV_F32_F80
                    ? LIR_FP_F32 : LIR_FP_F64;
                int dst_off = vreg_off(c, ins->dst);
                int src_phys = float_operand_phys(c, ins->a);

                if (src_phys >= 0) {
                    fprintf(c->out, "  %s %s, %d(%%rbp)\n",
                            fp_mov(srcw), xmm_name(src_phys), dst_off);
                    fprintf(c->out, "  fld%c %d(%%rbp)\n",
                            srcw == LIR_FP_F32 ? 's' : 'l', dst_off);
                } else {
                    assert(ins->a.kind == OPND_VREG);
                    fprintf(c->out, "  fld%c %d(%%rbp)\n",
                            srcw == LIR_FP_F32 ? 's' : 'l',
                            vreg_off(c, ins->a.u.vreg));
                }
                fprintf(c->out, "  fstpt %d(%%rbp)\n", dst_off);
                return;
            }
            if (ins->conv == CONV_F80_F32 || ins->conv == CONV_F80_F64) {
                LirFloatWidth dstw = ins->conv == CONV_F80_F32
                    ? LIR_FP_F32 : LIR_FP_F64;
                int target = vreg_phys(c, ins->dst);

                fprintf(c->out, "  fldt %d(%%rbp)\n",
                        f80_operand_off(c, ins->a));
                if (target < 0) {
                    fprintf(c->out, "  fstp%c %d(%%rbp)\n",
                            dstw == LIR_FP_F32 ? 's' : 'l',
                            vreg_off(c, ins->dst));
                } else {
                    fprintf(c->out,
                            "  sub $16, %%rsp\n"
                            "  fstp%c (%%rsp)\n"
                            "  %s (%%rsp), %s\n"
                            "  add $16, %%rsp\n",
                            dstw == LIR_FP_F32 ? 's' : 'l', fp_mov(dstw),
                            xmm_name(target));
                }
                return;
            }
            int dst_float = lir_vreg_class(c->lf, ins->dst) == REG_CLASS_XMM;
            if (dst_float) {
                int target = vreg_phys(c, ins->dst);
                if (target < 0)
                    target = PHYS_XMM15;
                if (ins->conv == CONV_F32_F64 || ins->conv == CONV_F64_F32) {
                    LirFloatWidth srcw = ins->conv == CONV_F32_F64
                        ? LIR_FP_F32 : LIR_FP_F64;
                    load_float_operand(c, ins->a, PHYS_XMM14, srcw);
                    fprintf(c->out, "  %s %s, %s\n",
                            ins->conv == CONV_F32_F64 ? "cvtss2sd" : "cvtsd2ss",
                            xmm_name(PHYS_XMM14), xmm_name(target));
                } else if (ins->conv == CONV_UI64_F32 ||
                           ins->conv == CONV_UI64_F64) {
                    const char *r0 = reg64_name(s0);
                    const char *r1 = reg64_name(s1);
                    char suffix = ins->fpw == LIR_FP_F32 ? 's' : 'd';

                    load_operand(c, ins->a, r0, LIR_W8);
                    fprintf(c->out,
                            "  test %s, %s\n"
                            "  jns 1f\n"
                            "  mov %s, %s\n"
                            "  and $1, %s\n"
                            "  shr %s\n"
                            "  or %s, %s\n"
                            "  cvtsi2s%cq %s, %s\n"
                            "  adds%c %s, %s\n"
                            "  jmp 2f\n"
                            "1:\n"
                            "  cvtsi2s%cq %s, %s\n"
                            "2:\n",
                            r0, r0, r0, r1, r0, r1, r0, r1,
                            suffix, r1, xmm_name(target), suffix,
                            xmm_name(target), xmm_name(target), suffix, r0,
                            xmm_name(target));
                } else {
                    int from64 = ins->conv == CONV_SI64_F32 ||
                                 ins->conv == CONV_SI64_F64 ||
                                 ins->conv == CONV_UI32_F32 ||
                                 ins->conv == CONV_UI32_F64;
                    load_operand(c, ins->a, "%rax", from64 ? LIR_W8 : LIR_W4);
                    fprintf(c->out, "  cvtsi2s%c%c %s, %s\n",
                            ins->fpw == LIR_FP_F32 ? 's' : 'd',
                            from64 ? 'q' : 'l', from64 ? "%rax" : "%eax",
                            xmm_name(target));
                }
                store_float(c, ins->dst, target, ins->fpw);
            } else {
                int from32 = ins->conv == CONV_F32_SI32 ||
                             ins->conv == CONV_F32_SI64 ||
                             ins->conv == CONV_F32_UI32 ||
                             ins->conv == CONV_F32_UI64;
                int to64 = ins->conv == CONV_F32_SI64 ||
                           ins->conv == CONV_F64_SI64 ||
                           ins->conv == CONV_F32_UI32 ||
                           ins->conv == CONV_F64_UI32 ||
                           ins->conv == CONV_F32_UI64 ||
                           ins->conv == CONV_F64_UI64;
                load_float_operand(c, ins->a, PHYS_XMM15,
                                   from32 ? LIR_FP_F32 : LIR_FP_F64);
                if (ins->conv == CONV_F32_UI64 ||
                    ins->conv == CONV_F64_UI64) {
                    const char *r0 = reg64_name(s0);
                    const char *r1 = reg64_name(s1);
                    const char *mov = from32 ? "movd" : "movq";
                    long threshold = from32 ? 0x5f000000L
                                            : 0x43e0000000000000L;

                    fprintf(c->out,
                            "  movabs $%ld, %s\n"
                            "  %s %s, %s\n"
                            "  ucomis%c %s, %s\n"
                            "  jb 1f\n"
                            "  subs%c %s, %s\n"
                            "  cvtts%c2siq %s, %s\n"
                            "  movabs $-9223372036854775808, %s\n"
                            "  xor %s, %s\n"
                            "  jmp 2f\n"
                            "1:\n"
                            "  cvtts%c2siq %s, %s\n"
                            "2:\n",
                            threshold, r0, mov, r0, xmm_name(PHYS_XMM14),
                            from32 ? 's' : 'd', xmm_name(PHYS_XMM14),
                            xmm_name(PHYS_XMM15), from32 ? 's' : 'd',
                            xmm_name(PHYS_XMM14), xmm_name(PHYS_XMM15),
                            from32 ? 's' : 'd', xmm_name(PHYS_XMM15), r0,
                            r1, r1, r0, from32 ? 's' : 'd',
                            xmm_name(PHYS_XMM15), r0);
                    store_vreg_slot(c, ins->dst, r0);
                } else {
                    fprintf(c->out, "  cvtts%c2si%c %s, %s\n",
                            from32 ? 's' : 'd', to64 ? 'q' : 'l',
                            xmm_name(PHYS_XMM15), to64 ? "%rax" : "%eax");
                    store_vreg_slot(c, ins->dst, "%rax");
                }
            }
            return;
        }
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
        default:
            assert(0 && "floating conversion reached integer conversion switch");
        }
        store_vreg_slot(c, ins->dst, "%rax");
        return;
    }

    case LIR_CALL: {
        int stack_size = lir_call_stack_size(ins);
        invalidate_rax(c);
        for (int i = ins->call_nreg; i < ins->nargs; i++) {
            Operand arg = ins->call_args[i];
            int off = lir_call_stack_offset(ins, i);
            if (ins->call_arg_types &&
                ins->call_arg_types[i] == LIR_TYPE_F80) {
                fprintf(c->out, "  fldt %d(%%rbp)\n  fstpt %d(%%rsp)\n",
                        f80_operand_off(c, arg), off);
            } else if (arg.kind == OPND_VREG &&
                lir_vreg_class(c->lf, arg.u.vreg) == REG_CLASS_XMM) {
                LirFloatWidth fpw = call_arg_float_width(c, ins, i);
                load_float_operand(c, arg, PHYS_XMM15, fpw);
                fprintf(c->out, "  %s %s, %d(%%rsp)\n", fp_mov(fpw),
                        xmm_name(PHYS_XMM15), off);
            } else {
                load_operand(c, arg, "%rax", LIR_W8);
                fprintf(c->out, "  mov %%rax, %d(%%rsp)\n", off);
            }
        }
        /* Stage vector arguments through reserved outgoing slots so register
           cycles cannot overwrite another argument's source. */
        for (int i = 0; i < ins->call_nsse; i++) {
            int index = ins->call_ngpr + i;
            Operand arg = ins->call_args[index];
            LirFloatWidth fpw = call_arg_float_width(c, ins, index);
            load_float_operand(c, arg, PHYS_XMM15, fpw);
            fprintf(c->out, "  %s %s, %d(%%rsp)\n", fp_mov(fpw),
                    xmm_name(PHYS_XMM15), stack_size + 8 * i);
        }
        for (int i = 0; i < ins->call_nsse; i++) {
            int index = ins->call_ngpr + i;
            LirFloatWidth fpw = call_arg_float_width(c, ins, index);
            fprintf(c->out, "  %s %d(%%rsp), %s\n", fp_mov(fpw),
                    stack_size + 8 * i, xmm_name(PHYS_XMM0 + i));
        }
        if (ins->call_indirect)
            load_operand(c, lir_vreg(ins->call_reg),
                         reg64_name(td->scratch1), LIR_W8);
        emit_call_register_args(c, ins);
        fprintf(c->out, "  mov $%d, %%al\n", ins->call_nsse);
        if (ins->call_indirect) {
            fprintf(c->out, "  call *%s\n", reg64_name(td->scratch1));
        } else {
            fprintf(c->out, "  call %s\n", ins->call_name);
        }
        if (ins->call_ret_type == LIR_TYPE_F80)
            fprintf(c->out, "  fstpt %d(%%rbp)\n", vreg_off(c, ins->dst));
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

static LirCond invert_cond(LirCond cc)
{
    switch (cc) {
    case CC_EQ: return CC_NE;
    case CC_NE: return CC_EQ;
    case CC_LT: return CC_GE;
    case CC_LE: return CC_GT;
    case CC_GT: return CC_LE;
    case CC_GE: return CC_LT;
    }
    return CC_EQ;
}

static int next_layout_block(const LirFn *lf, int block)
{
    for (int b = block + 1; b < lf->nblocks; b++) {
        if (b != lf->epilogue_label)
            return b;
    }
    if (block != lf->epilogue_label)
        return lf->epilogue_label;
    return LIR_NO_BLOCK;
}

static void emit_terminator(EmitCtx *c, const LirBlock *block, int next)
{
    const LirTerminator *term = &block->term;
    Instr ins = {0};

    if (term->kind == LIR_TERM_RET)
        return;
    if (term->kind == LIR_TERM_JMP) {
        if (term->target == next)
            return;
        ins.op = LIR_JMP;
        ins.label = term->target;
        emit_instr(c, &ins);
        return;
    }

    ins.op = LIR_BR;
    ins.a = term->a;
    ins.b = term->b;
    ins.w = term->w;
    ins.sgn = term->sgn;
    ins.fpw = term->fpw;
    if (term->false_target == next) {
        ins.cc = term->cc;
        ins.label = term->true_target;
        emit_instr(c, &ins);
        return;
    }
    if (term->true_target == next && term->fpw == LIR_FP_NONE) {
        ins.cc = invert_cond(term->cc);
        ins.label = term->false_target;
        emit_instr(c, &ins);
        return;
    }

    ins.cc = term->cc;
    ins.label = term->true_target;
    emit_instr(c, &ins);
    ins = (Instr){ .op = LIR_JMP, .label = term->false_target };
    emit_instr(c, &ins);
}

void emit_x86_function(LirFn *lf, Function *fn, AllocResult *alloc,
                       FILE *out, const TargetDesc *td)
{
    EmitCtx ctx = {
        .out = out, .lf = lf, .fn = fn, .alloc = alloc, .td = td,
        .rax_vreg = -1,
    };

    if (fn->linkage == LINKAGE_INTERNAL)
        fprintf(out, "  .local %s\n", fn->name);
    else
        fprintf(out, "  .globl %s\n", fn->name);
    fprintf(out, "  .type %s, @function\n", fn->name);
    fprintf(out, "%s:\n", fn->name);

    emit_prologue(&ctx);

    if (fn->variadic) {
        int off = fn->abi_vararg_save;

        for (int i = 0; i < 6; i++)
            fprintf(out, "  mov %s, %ld(%%rbp)\n",
                    reg64_name(td->arg_regs[i]), fp_disp(&ctx, off + i * 8));
        for (int i = 0; i < 8; i++)
            fprintf(out, "  movups %s, %ld(%%rbp)\n",
                    xmm_name(PHYS_XMM0 + i), fp_disp(&ctx, off + 48 + i * 16));
    }

    if (fn->abi_ret_sret)
        fprintf(out, "  mov %%rdi, %ld(%%rbp)\n",
                fp_disp(&ctx, fn->abi_sret_offset));

    for (Param *p = fn->params; p; p = p->next) {
        if (p->abi_sse_start >= 0) {
            fprintf(out, "  %s %s, %ld(%%rbp)\n",
                    type_same(type_unqualified(type_decay(p->ty)), type_float())
                        ? "movss" : "movsd",
                    xmm_name(PHYS_XMM0 + p->abi_sse_start),
                    fp_disp(&ctx, p->offset));
            continue;
        }
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

    for (int b = 0; b < lf->nblocks; b++) {
        LirBlock *block;
        int next;

        if (b == lf->epilogue_label)
            continue;
        block = &lf->blocks[b];
        emit_label_ref(&ctx, b);
        fprintf(out, ":\n");
        for (int i = 0; i < block->ninstr; i++)
            emit_instr(&ctx, &block->instrs[i]);
        next = next_layout_block(lf, b);
        emit_terminator(&ctx, block, next);
    }

    LirBlock *epilogue = &lf->blocks[lf->epilogue_label];
    emit_label_ref(&ctx, lf->epilogue_label);
    fprintf(out, ":\n");
    for (int i = 0; i < epilogue->ninstr; i++)
        emit_instr(&ctx, &epilogue->instrs[i]);

    emit_epilogue(&ctx);
    fprintf(out, "  .size %s, .-%s\n", fn->name, fn->name);
}
