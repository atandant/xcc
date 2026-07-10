/* SPDX-License-Identifier: MIT */
#include "lopt.h"

#include "loptstr.h"

#define MAX_ROUNDS 4

int lopt_imm_pow2_log2(long n)
{
    int k;

    if (n <= 0)
        return -1;
    k = 0;
    while ((n & 1) == 0) {
        n >>= 1;
        k++;
    }
    if (n != 1)
        return -1;
    return k;
}

static int loptconv_function(LirFn *lf)
{
    int changed = 0;

    for (int i = 1; i < lf->ninstr; i++) {
        Instr *load = &lf->instrs[i - 1];
        Instr *conv = &lf->instrs[i];

        if (conv->op != LIR_CONV || conv->conv != CONV_SEXT32_64)
            continue;
        if (load->op != LIR_LOAD || load->aux != 4)
            continue;
        if (load->dst != conv->dst)
            continue;
        if (conv->a.kind != OPND_VREG || conv->a.u.vreg != conv->dst)
            continue;

        conv->op = LIR_MOV;
        conv->conv = 0;
        changed = 1;
    }
    return changed;
}

void lopt_function(LirFn *lf, Function *fn)
{
    (void)fn;
    for (int round = 0; round < MAX_ROUNDS; round++) {
        int changed = 0;

        changed |= loptstr_function(lf);
        changed |= loptconv_function(lf);
        if (!changed)
            break;
    }
}
