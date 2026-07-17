/* SPDX-License-Identifier: MIT */
#include "loptstr.h"

#include "lopt.h"

static int imm_pow2_log2_operand(Operand op, int *log2_out)
{
  long imm;

  if (op.kind != OPND_IMM)
    return 0;
  imm = op.u.imm;
  if (imm <= 0)
    return 0;
  *log2_out = lopt_imm_pow2_log2(imm);
  return *log2_out >= 0;
}

int loptstr_function(LirFn *lf)
{
    int changed = 0;

    for (int b = 0; b < lf->nblocks; b++) {
      for (int i = 0; i < lf->blocks[b].ninstr; i++) {
        Instr *ins = &lf->blocks[b].instrs[i];
        int k;

        if (ins->op == LIR_MUL && imm_pow2_log2_operand(ins->b, &k)) {
            ins->op = LIR_SHL;
            ins->b = lir_imm(k);
            changed = 1;
            continue;
        }

        if (ins->op == LIR_DIV || ins->op == LIR_MOD) {
            if (!imm_pow2_log2_operand(ins->b, &k))
                continue;

            if (ins->sgn == LIR_SGN_U) {
                ins->op = (ins->op == LIR_DIV) ? LIR_UDIV_POW2 : LIR_UMOD_POW2;
            } else {
                ins->op = (ins->op == LIR_DIV) ? LIR_SDIV_POW2 : LIR_SMOD_POW2;
            }
            ins->aux = k;
            ins->b = lir_none();
            changed = 1;
        }
      }
    }
    return changed;
}
