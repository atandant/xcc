/* SPDX-License-Identifier: MIT */
#include "lir_copy_prop.h"

#include "arena.h"

#include <string.h>

static int is_f80_identity_mov(const Instr *ins)
{
    return ins->op == LIR_MOV && ins->dst >= 0 &&
           ins->a.kind == OPND_VREG && ins->a.u.vreg == ins->dst;
}

static int is_f80_copy_mov(const LirFn *fn, const Instr *ins)
{
    return ins->op == LIR_MOV && ins->dst >= 0 &&
           ins->a.kind == OPND_VREG &&
           lir_vreg_type(fn, ins->dst) == LIR_TYPE_F80;
}

static int resolve_alias(int *alias, int vreg)
{
    int root = vreg;

    while (alias[root] != root)
        root = alias[root];
    while (alias[vreg] != vreg) {
        int next = alias[vreg];
        alias[vreg] = root;
        vreg = next;
    }
    return root;
}

static void rewrite_operand(Operand *operand, int *alias)
{
    if (operand->kind == OPND_VREG)
        operand->u.vreg = resolve_alias(alias, operand->u.vreg);
    else if (operand->kind == OPND_MEM) {
        if (operand->u.mem.base != LIR_FP)
            operand->u.mem.base = resolve_alias(alias, operand->u.mem.base);
        if (operand->u.mem.index != LIR_NO_IDX)
            operand->u.mem.index = resolve_alias(alias, operand->u.mem.index);
    }
}

static void rewrite_instruction(Instr *ins, int *alias)
{
    rewrite_operand(&ins->a, alias);
    rewrite_operand(&ins->b, alias);
    if (ins->op == LIR_CALL) {
        if (ins->call_indirect)
            ins->call_reg = resolve_alias(alias, ins->call_reg);
        for (int a = 0; a < ins->nargs; a++)
            rewrite_operand(&ins->call_args[a], alias);
    }
}

static void rewrite_phis(LirFn *fn, int *alias)
{
    for (int b = 0; b < fn->nblocks; b++) {
        LirBlock *block = &fn->blocks[b];

        for (int p = 0; p < block->nphis; p++) {
            LirPhi *phi = &block->phis[p];

            for (int i = 0; i < phi->ninputs; i++)
                phi->inputs[i].value = resolve_alias(alias, phi->inputs[i].value);
        }
    }
}

static void rewrite_terminators(LirFn *fn, int *alias)
{
    for (int b = 0; b < fn->nblocks; b++) {
        LirTerminator *term = &fn->blocks[b].term;

        rewrite_operand(&term->a, alias);
        rewrite_operand(&term->b, alias);
    }
}

static int sweep_copy_moves(LirFn *fn, const int *def_count)
{
    int changed = 0;

    for (int b = 0; b < fn->nblocks; b++) {
        LirBlock *block = &fn->blocks[b];
        int out = 0;

        for (int i = 0; i < block->ninstr; i++) {
            Instr ins = block->instrs[i];

            if ((is_f80_identity_mov(&ins) ||
                 (is_f80_copy_mov(fn, &ins) && def_count[ins.dst] == 1))) {
                changed = 1;
                continue;
            }
            block->instrs[out++] = ins;
        }
        block->ninstr = out;
    }
    return changed;
}

int lir_propagate_f80_copies(LirFn *fn)
{
    int nvreg = fn->nvreg;
    int *def_count;
    int *alias;
    int changed = 0;

    if (nvreg <= 0)
        return 0;

    def_count = arena_alloc_zeroed((size_t)nvreg * sizeof(*def_count));
    alias = arena_alloc((size_t)nvreg * sizeof(*alias));
    for (int v = 0; v < nvreg; v++)
        alias[v] = v;

    for (int b = 0; b < fn->nblocks; b++) {
        const LirBlock *block = &fn->blocks[b];

        for (int p = 0; p < block->nphis; p++) {
            int dst = block->phis[p].dst;

            if (dst >= 0 && dst < nvreg)
                def_count[dst]++;
        }
        for (int i = 0; i < block->ninstr; i++) {
            const Instr *ins = &block->instrs[i];

            if (lir_instruction_defines_vreg(ins) && ins->dst < nvreg)
                def_count[ins->dst]++;
        }
    }

    for (int b = 0; b < fn->nblocks; b++) {
        const LirBlock *block = &fn->blocks[b];

        for (int i = 0; i < block->ninstr; i++) {
            const Instr *ins = &block->instrs[i];

            if (is_f80_identity_mov(ins)) {
                changed = 1;
                continue;
            }
            if (!is_f80_copy_mov(fn, ins) || def_count[ins->dst] != 1)
                continue;
            alias[ins->dst] = resolve_alias(alias, ins->a.u.vreg);
            changed = 1;
        }
    }

    if (!changed)
        return 0;

    for (int b = 0; b < fn->nblocks; b++) {
        LirBlock *block = &fn->blocks[b];

        for (int i = 0; i < block->ninstr; i++)
            rewrite_instruction(&block->instrs[i], alias);
    }
    rewrite_phis(fn, alias);
    rewrite_terminators(fn, alias);
    sweep_copy_moves(fn, def_count);
    return 1;
}
