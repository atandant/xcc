/* SPDX-License-Identifier: MIT */
#include "lir_algebraic_simplify.h"

#include <stdlib.h>

static int operand_constant(Operand op, const unsigned char *known,
                            const long *values, long *value)
{
    if (op.kind == OPND_IMM) {
        *value = op.u.imm;
        return 1;
    }
    if (op.kind == OPND_VREG && known[op.u.vreg]) {
        *value = values[op.u.vreg];
        return 1;
    }
    return 0;
}

static int same_vreg(Operand a, Operand b)
{
    return a.kind == OPND_VREG && b.kind == OPND_VREG &&
           a.u.vreg == b.u.vreg;
}

static int all_ones(long value, LirWidth width)
{
    if (width == LIR_W4)
        return ((unsigned long)value & 0xffffffffUL) == 0xffffffffUL;
    return value == -1L;
}

static void replace_with_move(Instr *ins, Operand value)
{
    ins->op = LIR_MOV;
    ins->a = value;
    ins->b = lir_none();
}

static void replace_with_constant(Instr *ins, long value)
{
    ins->op = LIR_MOVI;
    ins->a = lir_imm(value);
    ins->b = lir_none();
}

static int simplify_instruction(Instr *ins, const unsigned char *known,
                                const long *values)
{
    long a = 0;
    long b = 0;
    int a_const = operand_constant(ins->a, known, values, &a);
    int b_const = operand_constant(ins->b, known, values, &b);

    switch (ins->op) {
    case LIR_ADD:
        if (b_const && b == 0)
            replace_with_move(ins, ins->a);
        else if (a_const && a == 0)
            replace_with_move(ins, ins->b);
        else
            return 0;
        return 1;
    case LIR_SUB:
        /* A shared vreg denotes one already-evaluated value.  Do not extend
           this to separate loads without alias and volatile analysis. */
        if (same_vreg(ins->a, ins->b))
            replace_with_constant(ins, 0);
        else if (b_const && b == 0)
            replace_with_move(ins, ins->a);
        else
            return 0;
        return 1;
    case LIR_MUL:
        if ((a_const && a == 0) || (b_const && b == 0))
            replace_with_constant(ins, 0);
        else if (b_const && b == 1)
            replace_with_move(ins, ins->a);
        else if (a_const && a == 1)
            replace_with_move(ins, ins->b);
        else
            return 0;
        return 1;
    case LIR_DIV:
        if (b_const && b == 1)
            replace_with_move(ins, ins->a);
        else
            return 0;
        return 1;
    case LIR_MOD:
        if (b_const && b == 1)
            replace_with_constant(ins, 0);
        else
            return 0;
        return 1;
    case LIR_AND:
        if ((a_const && a == 0) || (b_const && b == 0))
            replace_with_constant(ins, 0);
        else if (b_const && all_ones(b, ins->w))
            replace_with_move(ins, ins->a);
        else if (a_const && all_ones(a, ins->w))
            replace_with_move(ins, ins->b);
        else
            return 0;
        return 1;
    case LIR_OR:
        if (b_const && b == 0)
            replace_with_move(ins, ins->a);
        else if (a_const && a == 0)
            replace_with_move(ins, ins->b);
        else
            return 0;
        return 1;
    case LIR_XOR:
        if (same_vreg(ins->a, ins->b) ||
            ((a_const && a == 0) && (b_const && b == 0)))
            replace_with_constant(ins, 0);
        else if (b_const && b == 0)
            replace_with_move(ins, ins->a);
        else if (a_const && a == 0)
            replace_with_move(ins, ins->b);
        else
            return 0;
        return 1;
    case LIR_SHL:
    case LIR_SHR:
    case LIR_SAR:
        if (b_const && b == 0)
            replace_with_move(ins, ins->a);
        else
            return 0;
        return 1;
    case LIR_SDIV_POW2:
    case LIR_UDIV_POW2:
        if (ins->aux == 0)
            replace_with_move(ins, ins->a);
        else
            return 0;
        return 1;
    case LIR_SMOD_POW2:
    case LIR_UMOD_POW2:
        if (ins->aux == 0)
            replace_with_constant(ins, 0);
        else
            return 0;
        return 1;
    default:
        return 0;
    }
}

int lir_algebraic_simplify_function(LirFn *lf)
{
    int changed = 0;

    for (int b = 0; b < lf->nblocks; b++) {
        LirBlock *block = &lf->blocks[b];
        size_t nvalues = lf->nvreg > 0 ? (size_t)lf->nvreg : 1;
        unsigned char *known = calloc(nvalues, sizeof(*known));
        long *values = calloc(nvalues, sizeof(*values));

        if (!known || !values)
            abort();
        for (int i = 0; i < block->ninstr; i++) {
            Instr *ins = &block->instrs[i];

            changed |= simplify_instruction(ins, known, values);
            if (ins->op == LIR_MOVI && ins->dst >= 0) {
                known[ins->dst] = 1;
                values[ins->dst] = ins->a.u.imm;
            } else if (ins->op == LIR_MOV && ins->dst >= 0) {
                long value;

                known[ins->dst] = operand_constant(ins->a, known, values, &value);
                if (known[ins->dst])
                    values[ins->dst] = value;
            } else if (ins->dst >= 0 && ins->dst < lf->nvreg) {
                known[ins->dst] = 0;
            }
        }
        free(values);
        free(known);
    }
    return changed;
}
