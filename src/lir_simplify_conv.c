/* SPDX-License-Identifier: MIT */
#include "lir_simplify_conv.h"

int lir_simplify_conversions_function(LirFn *lf)
{
    int changed = 0;

    for (int b = 0; b < lf->nblocks; b++) {
        LirBlock *block = &lf->blocks[b];
        for (int i = 1; i < block->ninstr; i++) {
            Instr *load = &block->instrs[i - 1];
            Instr *conv = &block->instrs[i];

            if (conv->op != LIR_CONV || conv->conv != CONV_SEXT32_64)
                continue;
            if (load->op != LIR_LOAD || load->aux != 4)
                continue;
            if (conv->a.kind != OPND_VREG || conv->a.u.vreg != load->dst)
                continue;

            conv->op = LIR_MOV;
            conv->conv = 0;
            changed = 1;
        }
    }
    return changed;
}
