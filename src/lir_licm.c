/* SPDX-License-Identifier: MIT */
#include "lir_licm.h"

#include "arena.h"
#include "lir_dom.h"

typedef struct {
    int header;
    int preheader;
    int nmembers;
    unsigned char *members;
} NaturalLoop;

static int loop_for_header(NaturalLoop *loops, int nloops, int header)
{
    for (int i = 0; i < nloops; i++) {
        if (loops[i].header == header)
            return i;
    }
    return -1;
}

static void add_natural_loop(const LirFn *fn, NaturalLoop *loop, int latch)
{
    int *work = arena_alloc((size_t)fn->nblocks * sizeof(*work));
    int nwork = 0;

    if (!loop->members[loop->header])
        loop->members[loop->header] = 1;
    if (!loop->members[latch]) {
        loop->members[latch] = 1;
        if (latch != loop->header)
            work[nwork++] = latch;
    }

    while (nwork > 0) {
        const LirBlock *block = &fn->blocks[work[--nwork]];

        for (int p = 0; p < block->npreds; p++) {
            int pred = block->preds[p];

            if (!loop->members[pred]) {
                loop->members[pred] = 1;
                if (pred != loop->header)
                    work[nwork++] = pred;
            }
        }
    }
}

static void consider_back_edge(const LirFn *fn, const LirDom *dom,
                               NaturalLoop *loops, int *nloops,
                               int latch, int header)
{
    int index;

    if (!lir_dom_dominates(dom, header, latch))
        return;
    index = loop_for_header(loops, *nloops, header);
    if (index < 0) {
        index = (*nloops)++;
        loops[index].header = header;
        loops[index].preheader = LIR_NO_BLOCK;
        loops[index].members =
            arena_alloc_zeroed((size_t)fn->nblocks);
    }
    add_natural_loop(fn, &loops[index], latch);
}

static int find_preheader(const LirFn *fn, const LirDom *dom,
                          const NaturalLoop *loop)
{
    const LirBlock *header = &fn->blocks[loop->header];
    int preheader = LIR_NO_BLOCK;

    for (int p = 0; p < header->npreds; p++) {
        int pred = header->preds[p];

        if (loop->members[pred])
            continue;
        if (preheader != LIR_NO_BLOCK)
            return LIR_NO_BLOCK;
        preheader = pred;
    }
    if (preheader == LIR_NO_BLOCK || !dom->reachable[preheader])
        return LIR_NO_BLOCK;
    if (fn->blocks[preheader].term.kind != LIR_TERM_JMP ||
        fn->blocks[preheader].term.target != loop->header)
        return LIR_NO_BLOCK;
    return preheader;
}

static void sort_inner_loops_first(NaturalLoop *loops, int nloops)
{
    for (int i = 0; i < nloops; i++) {
        int smallest = i;

        for (int j = i + 1; j < nloops; j++) {
            if (loops[j].nmembers < loops[smallest].nmembers)
                smallest = j;
        }
        if (smallest != i) {
            NaturalLoop tmp = loops[i];
            loops[i] = loops[smallest];
            loops[smallest] = tmp;
        }
    }
}

static int safe_to_speculate(const Instr *ins)
{
    switch (ins->op) {
    case LIR_LEA:
    case LIR_LEA_SYM:
    case LIR_ADD:
    case LIR_SUB:
    case LIR_MUL:
    case LIR_AND:
    case LIR_XOR:
    case LIR_OR:
    case LIR_SHL:
    case LIR_SHR:
    case LIR_SAR:
    case LIR_NEG:
    case LIR_SETCC:
    case LIR_CONV:
        return ins->dst != LIR_NO_VREG;
    default:
        return 0;
    }
}

static int invariant_vreg(int vreg, const NaturalLoop *loop,
                          const int *def_block,
                          const unsigned char *selected, int nvreg)
{
    if (vreg < 0 || vreg >= nvreg || def_block[vreg] < 0)
        return 0;
    return !loop->members[def_block[vreg]] || selected[vreg];
}

static int invariant_operand(Operand operand, int allow_address,
                             const NaturalLoop *loop, const int *def_block,
                             const unsigned char *selected, int nvreg)
{
    switch (operand.kind) {
    case OPND_NONE:
    case OPND_IMM:
        return 1;
    case OPND_VREG:
        return invariant_vreg(operand.u.vreg, loop, def_block,
                              selected, nvreg);
    case OPND_MEM:
        if (!allow_address)
            return 0;
        if (operand.u.mem.base != LIR_FP &&
            !invariant_vreg(operand.u.mem.base, loop, def_block,
                            selected, nvreg))
            return 0;
        return operand.u.mem.index == LIR_NO_IDX ||
               invariant_vreg(operand.u.mem.index, loop, def_block,
                               selected, nvreg);
    case OPND_PHYS:
        return 0;
    }
    return 0;
}

static int invariant_instruction(const Instr *ins, const NaturalLoop *loop,
                                 const int *def_block,
                                 const unsigned char *selected, int nvreg)
{
    int address = ins->op == LIR_LEA;

    return safe_to_speculate(ins) &&
           invariant_operand(ins->a, address, loop, def_block,
                             selected, nvreg) &&
           invariant_operand(ins->b, 0, loop, def_block,
                             selected, nvreg);
}

static void find_def_blocks(const LirFn *fn, int *def_block)
{
    for (int v = 0; v < fn->nvreg; v++)
        def_block[v] = LIR_NO_BLOCK;
    for (int b = 0; b < fn->nblocks; b++) {
        const LirBlock *block = &fn->blocks[b];

        for (int p = 0; p < block->nphis; p++)
            def_block[block->phis[p].dst] = b;
        for (int i = 0; i < block->ninstr; i++) {
            if (block->instrs[i].dst != LIR_NO_VREG)
                def_block[block->instrs[i].dst] = b;
        }
    }
}

static int hoist_loop(LirFn *fn, const NaturalLoop *loop)
{
    size_t nvreg = fn->nvreg > 0 ? (size_t)fn->nvreg : 1;
    int *def_block = arena_alloc(nvreg * sizeof(*def_block));
    unsigned char *selected = arena_alloc_zeroed(nvreg);
    int ninstr = 0;
    int nmoves = 0;
    int changed;

    find_def_blocks(fn, def_block);
    for (int b = 0; b < fn->nblocks; b++)
        ninstr += fn->blocks[b].ninstr;
    Instr *moves = arena_alloc((size_t)(ninstr > 0 ? ninstr : 1) *
                               sizeof(*moves));

    do {
        changed = 0;
        for (int b = 0; b < fn->nblocks; b++) {
            LirBlock *block;

            if (!loop->members[b])
                continue;
            block = &fn->blocks[b];
            for (int i = 0; i < block->ninstr; i++) {
                Instr *ins = &block->instrs[i];

                if (ins->dst < 0 || ins->dst >= fn->nvreg ||
                    selected[ins->dst])
                    continue;
                if (invariant_instruction(ins, loop, def_block,
                                          selected, fn->nvreg)) {
                    selected[ins->dst] = 1;
                    moves[nmoves++] = *ins;
                    changed = 1;
                }
            }
        }
    } while (changed);

    if (nmoves == 0)
        return 0;

    for (int b = 0; b < fn->nblocks; b++) {
        LirBlock *block;
        int out = 0;

        if (!loop->members[b])
            continue;
        block = &fn->blocks[b];
        for (int i = 0; i < block->ninstr; i++) {
            Instr ins = block->instrs[i];

            if (ins.dst >= 0 && ins.dst < fn->nvreg && selected[ins.dst])
                continue;
            block->instrs[out++] = ins;
        }
        block->ninstr = out;
    }
    for (int i = 0; i < nmoves; i++)
        lir_block_emit(&fn->blocks[loop->preheader], moves[i]);
    return 1;
}

int lir_licm_function(LirFn *fn)
{
    LirDom dom;
    NaturalLoop *loops;
    int nloops = 0;
    int changed = 0;

    if (fn->stage != LIR_STAGE_SSA || fn->nblocks == 0)
        return 0;
    lir_dom_compute(fn, &dom);
    loops = arena_alloc_zeroed((size_t)fn->nblocks * sizeof(*loops));

    for (int b = 0; b < fn->nblocks; b++) {
        const LirTerminator *term;

        if (!dom.reachable[b])
            continue;
        term = &fn->blocks[b].term;
        if (term->kind == LIR_TERM_JMP) {
            consider_back_edge(fn, &dom, loops, &nloops, b, term->target);
        } else if (term->kind == LIR_TERM_BR) {
            consider_back_edge(fn, &dom, loops, &nloops, b,
                               term->true_target);
            if (term->false_target != term->true_target)
                consider_back_edge(fn, &dom, loops, &nloops, b,
                                   term->false_target);
        }
    }

    for (int l = 0; l < nloops; l++) {
        for (int b = 0; b < fn->nblocks; b++)
            loops[l].nmembers += loops[l].members[b] != 0;
        loops[l].preheader = find_preheader(fn, &dom, &loops[l]);
    }
    sort_inner_loops_first(loops, nloops);
    for (int l = 0; l < nloops; l++) {
        if (loops[l].preheader != LIR_NO_BLOCK)
            changed |= hoist_loop(fn, &loops[l]);
    }
    return changed;
}
