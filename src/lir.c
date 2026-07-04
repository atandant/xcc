/* SPDX-License-Identifier: MIT */
#include "lir.h"
#include "arena.h"

#include <assert.h>
#include <string.h>

Operand lir_vreg(int v)
{
    Operand o = { .kind = OPND_VREG, .u.vreg = v };
    return o;
}

Operand lir_phys(int r)
{
    Operand o = { .kind = OPND_PHYS, .u.phys = r };
    return o;
}

Operand lir_imm(long imm)
{
    Operand o = { .kind = OPND_IMM, .u.imm = imm };
    return o;
}

Operand lir_mem(int base, long disp)
{
    Operand o = { .kind = OPND_MEM };
    o.u.mem.base = base;
    o.u.mem.disp = disp;
    o.u.mem.index = LIR_NO_IDX;
    o.u.mem.scale = 1;
    return o;
}

Operand lir_mem_idx(int base, int index, int scale, long disp)
{
    Operand o = { .kind = OPND_MEM };
    o.u.mem.base = base;
    o.u.mem.disp = disp;
    o.u.mem.index = index;
    o.u.mem.scale = scale;
    return o;
}

Operand lir_none(void)
{
    Operand o = { .kind = OPND_NONE };
    return o;
}

LirFn *lir_fn_new(const char *name)
{
    LirFn *fn = arena_alloc_zeroed(sizeof(*fn));
    fn->name = arena_strdup(name);
    fn->cap = 64;
    fn->instrs = arena_alloc((size_t)fn->cap * sizeof(*fn->instrs));
    fn->loops_cap = 8;
    fn->loops = arena_alloc((size_t)fn->loops_cap * sizeof(*fn->loops));
    fn->homes_cap = 32;
    fn->homes = arena_alloc((size_t)fn->homes_cap * sizeof(*fn->homes));
    return fn;
}

int lir_home_vreg(LirFn *lf, int offset)
{
    for (int i = 0; i < lf->nhomes; i++) {
        if (lf->homes[i].offset == offset)
            return lf->homes[i].vreg;
    }
    return LIR_NO_VREG;
}

int lir_is_home_vreg(const LirFn *lf, int vreg)
{
    for (int i = 0; i < lf->nhomes; i++) {
        if (lf->homes[i].vreg == vreg)
            return 1;
    }
    return 0;
}

void lir_bind_home(LirFn *lf, int offset, int vreg)
{
    for (int i = 0; i < lf->nhomes; i++) {
        if (lf->homes[i].offset == offset) {
            lf->homes[i].vreg = vreg;
            return;
        }
    }
    if (lf->nhomes >= lf->homes_cap) {
        lf->homes_cap *= 2;
        LocalHome *n = arena_alloc((size_t)lf->homes_cap * sizeof(*n));
        memcpy(n, lf->homes, (size_t)lf->nhomes * sizeof(*n));
        lf->homes = n;
    }
    lf->homes[lf->nhomes].offset = offset;
    lf->homes[lf->nhomes].vreg = vreg;
    lf->nhomes++;
}

int lir_max_outgoing(const LirFn *lf)
{
    int max_out = 0;

    for (int i = 0; i < lf->ninstr; i++) {
        const Instr *ins = &lf->instrs[i];

        if (ins->op != LIR_CALL)
            continue;
        if (ins->nargs <= 6)
            continue;
        int out = 8 * (ins->nargs - 6);
        if (out > max_out)
            max_out = out;
    }
    return max_out;
}

int lir_new_vreg(LirFn *fn)
{
    return fn->nvreg++;
}

int lir_new_label(LirFn *fn)
{
    return fn->label_count++;
}

void lir_add_loop(LirFn *fn, int begin, int end)
{
    if (fn->nloops >= fn->loops_cap) {
        fn->loops_cap *= 2;
        LoopRange *n = arena_alloc((size_t)fn->loops_cap * sizeof(*n));
        memcpy(n, fn->loops, (size_t)fn->nloops * sizeof(*n));
        fn->loops = n;
    }
    fn->loops[fn->nloops].begin = begin;
    fn->loops[fn->nloops].end = end;
    fn->nloops++;
}

int lir_emit(LirFn *fn, Instr ins)
{
    if (fn->ninstr >= fn->cap) {
        fn->cap *= 2;
        Instr *n = arena_alloc((size_t)fn->cap * sizeof(*n));
        memcpy(n, fn->instrs, (size_t)fn->ninstr * sizeof(*n));
        fn->instrs = n;
    }
    int idx = fn->ninstr++;
    fn->instrs[idx] = ins;
    return idx;
}

static const char *op_name(LirOp op)
{
    switch (op) {
    case LIR_MOVI:  return "movi";
    case LIR_MOV:   return "mov";
    case LIR_LOAD:  return "load";
    case LIR_STORE: return "store";
    case LIR_LEA:   return "lea";
    case LIR_ADD:   return "add";
    case LIR_SUB:   return "sub";
    case LIR_MUL:   return "mul";
    case LIR_DIV:   return "div";
    case LIR_MOD:   return "mod";
    case LIR_NEG:   return "neg";
    case LIR_SETCC: return "setcc";
    case LIR_BR:    return "br";
    case LIR_JMP:   return "jmp";
    case LIR_LABEL: return "label";
    case LIR_CONV:  return "conv";
    case LIR_CALL:  return "call";
    case LIR_RET:   return "ret";
    }
    return "?";
}

static const char *cc_name(LirCond cc)
{
    switch (cc) {
    case CC_EQ: return "eq";
    case CC_NE: return "ne";
    case CC_LT: return "lt";
    case CC_LE: return "le";
    case CC_GT: return "gt";
    case CC_GE: return "ge";
    }
    return "?";
}

static const char *conv_name(ConvKind k)
{
    switch (k) {
    case CONV_ZEXT8:     return "zext8";
    case CONV_SEXT8:     return "sext8";
    case CONV_SEXT16:    return "sext16";
    case CONV_SEXT32_64: return "sext32_64";
    case CONV_TRUNC_LO32: return "trunc_lo32";
    }
    return "?";
}

static void dump_operand(FILE *out, Operand o)
{
    switch (o.kind) {
    case OPND_NONE:
        fprintf(out, "_");
        return;
    case OPND_VREG:
        fprintf(out, "v%d", o.u.vreg);
        return;
    case OPND_PHYS:
        fprintf(out, "p%d", o.u.phys);
        return;
    case OPND_IMM:
        fprintf(out, "%ld", o.u.imm);
        return;
    case OPND_MEM:
        fprintf(out, "[");
        if (o.u.mem.base == LIR_FP)
            fprintf(out, "fp");
        else
            fprintf(out, "v%d", o.u.mem.base);
        if (o.u.mem.index != LIR_NO_IDX)
            fprintf(out, "+v%d*%d", o.u.mem.index, o.u.mem.scale);
        if (o.u.mem.disp)
            fprintf(out, "%+ld", o.u.mem.disp);
        fprintf(out, "]");
        return;
    }
}

void lir_dump_fn(LirFn *fn, FILE *out)
{
    fprintf(out, "function %s (nvreg=%d)\n", fn->name, fn->nvreg);
    for (int i = 0; i < fn->ninstr; i++) {
        Instr *ins = &fn->instrs[i];
        fprintf(out, "  %4d  %-6s", i, op_name(ins->op));
        if (ins->dst != LIR_NO_VREG)
            fprintf(out, " v%d,", ins->dst);
        switch (ins->op) {
        case LIR_MOVI:
            dump_operand(out, ins->a);
            break;
        case LIR_MOV:
        case LIR_LOAD:
        case LIR_NEG:
        case LIR_CONV:
            dump_operand(out, ins->a);
            if (ins->op == LIR_LOAD)
                fprintf(out, " %s", ins->sgn == LIR_SGN_Z ? "z" : "s");
            if (ins->op == LIR_CONV)
                fprintf(out, " %s", conv_name(ins->conv));
            break;
        case LIR_STORE:
            dump_operand(out, ins->a);
            fprintf(out, ", ");
            dump_operand(out, ins->b);
            break;
        case LIR_LEA:
            dump_operand(out, ins->a);
            break;
        case LIR_ADD:
        case LIR_SUB:
        case LIR_MUL:
        case LIR_DIV:
        case LIR_MOD:
        case LIR_SETCC:
            dump_operand(out, ins->a);
            fprintf(out, ", ");
            dump_operand(out, ins->b);
            fprintf(out, " %c", ins->w == LIR_W4 ? '4' : '8');
            if (ins->op == LIR_SETCC)
                fprintf(out, " %s", cc_name(ins->cc));
            break;
        case LIR_BR:
            fprintf(out, "%s ", cc_name(ins->cc));
            dump_operand(out, ins->a);
            fprintf(out, ", ");
            dump_operand(out, ins->b);
            fprintf(out, " %c -> L%d", ins->w == LIR_W4 ? '4' : '8', ins->label);
            break;
        case LIR_JMP:
        case LIR_LABEL:
            fprintf(out, "L%d", ins->label);
            break;
        case LIR_CALL:
            fprintf(out, "%s(", ins->call_name);
            for (int a = 0; a < ins->nargs; a++) {
                if (a)
                    fprintf(out, ", ");
                dump_operand(out, ins->call_args[a]);
            }
            fprintf(out, ")");
            break;
        case LIR_RET:
            dump_operand(out, ins->a);
            break;
        }
        fprintf(out, "\n");
    }
    for (int i = 0; i < fn->nloops; i++)
        fprintf(out, "  loop [%d, %d]\n", fn->loops[i].begin, fn->loops[i].end);
}
