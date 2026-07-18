/* SPDX-License-Identifier: MIT */
#include "lir_strength_reduce.h"

#include <stdlib.h>

static int imm_pow2_log2(long n)
{
    int k;

    if (n <= 0)
        return -1;
    k = 0;
    while ((n & 1) == 0) {
        n >>= 1;
        k++;
    }
    return n == 1 ? k : -1;
}

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

static int operand_pow2_log2(Operand op, const unsigned char *known,
                             const long *values, int *log2_out)
{
    long value;

    if (!operand_constant(op, known, values, &value))
        return 0;
    *log2_out = imm_pow2_log2(value);
    return *log2_out >= 0;
}

int lir_strength_reduce_function(LirFn *lf)
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
            int k;

            if (ins->op == LIR_MUL) {
                if (operand_pow2_log2(ins->b, known, values, &k)) {
                    ins->op = LIR_SHL;
                    ins->b = lir_imm(k);
                    changed = 1;
                } else if (operand_pow2_log2(ins->a, known, values, &k)) {
                    ins->op = LIR_SHL;
                    ins->a = ins->b;
                    ins->b = lir_imm(k);
                    changed = 1;
                }
            }

            if ((ins->op == LIR_DIV || ins->op == LIR_MOD) &&
                operand_pow2_log2(ins->b, known, values, &k)) {
                if (ins->sgn == LIR_SGN_U) {
                    ins->op = (ins->op == LIR_DIV) ? LIR_UDIV_POW2 : LIR_UMOD_POW2;
                } else {
                    ins->op = (ins->op == LIR_DIV) ? LIR_SDIV_POW2 : LIR_SMOD_POW2;
                }
                ins->aux = k;
                ins->b = lir_none();
                changed = 1;
            }

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
