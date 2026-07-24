/* SPDX-License-Identifier: MIT */
#include "liveness.h"
#include "arena.h"
#include "diag.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_REG_OPS 16

static void add_use(int *uses, int *nu, int v)
{
    if (v < 0)
        return;
    for (int i = 0; i < *nu; i++)
        if (uses[i] == v)
            return;
    assert(*nu < MAX_REG_OPS);
    uses[(*nu)++] = v;
}

static void add_def(int *defs, int *nd, int v)
{
    if (v < 0)
        return;
    for (int i = 0; i < *nd; i++)
        if (defs[i] == v)
            return;
    assert(*nd < MAX_REG_OPS);
    defs[(*nd)++] = v;
}

static void touch_vreg_use(Liveness *lv, int v, int idx)
{
    if (v < 0)
        return;
    if (lv->by_vreg[v].end < idx)
        lv->by_vreg[v].end = idx;
    if (lv->by_vreg[v].start > idx)
        lv->by_vreg[v].start = idx;
}

static void touch_vreg_def(Liveness *lv, int v, int idx)
{
    if (v < 0)
        return;
    if (lv->by_vreg[v].start > idx)
        lv->by_vreg[v].start = idx;
}

static void block_phys(Liveness *lv, int phys, int idx)
{
    if (phys < 0 || phys >= PHYS_COUNT)
        return;
    PhysBlocks *pb = &lv->phys[phys];
    if (pb->npts >= pb->cap) {
        pb->cap = pb->cap ? pb->cap * 2 : 16;
        int *n = arena_alloc((size_t)pb->cap * sizeof(*n));
        if (pb->pts)
            memcpy(n, pb->pts, (size_t)pb->npts * sizeof(*n));
        pb->pts = n;
    }
    pb->pts[pb->npts++] = idx;
}

static void block_caller_saved(Liveness *lv, const TargetDesc *td, int idx)
{
    for (int r = 0; r < PHYS_COUNT; r++) {
        if (td->caller_saved_mask & (1u << r))
            block_phys(lv, r, idx);
    }
}

static void operand_vreg_uses(Operand o, int *uses, int *nu)
{
    switch (o.kind) {
    case OPND_VREG:
        add_use(uses, nu, o.u.vreg);
        return;
    case OPND_MEM:
        if (o.u.mem.base >= 0)
            add_use(uses, nu, o.u.mem.base);
        if (o.u.mem.index >= 0 && o.u.mem.index != LIR_NO_IDX)
            add_use(uses, nu, o.u.mem.index);
        return;
    default:
        return;
    }
}

static void operand_phys_touch(Liveness *lv, Operand o, int idx)
{
    if (o.kind == OPND_PHYS)
        block_phys(lv, o.u.phys, idx);
}

static void instr_use_def(Instr *ins, int i, const TargetDesc *td, Liveness *lv,
                          int *uses, int *nu, int *defs, int *nd)
{
    *nu = 0;
    *nd = 0;

    switch (ins->op) {
    case LIR_LABEL:
    case LIR_JMP:
        return;

    case LIR_MOVI:
        add_def(defs, nd, ins->dst);
        return;

    case LIR_MOV:
        operand_vreg_uses(ins->a, uses, nu);
        operand_phys_touch(lv, ins->a, i);
        if (ins->dst >= 0) {
            add_def(defs, nd, ins->dst);
            return;
        }
        operand_phys_touch(lv, ins->b, i);
        return;

    case LIR_LOAD:
    case LIR_NEG:
    case LIR_CONV:
        operand_vreg_uses(ins->a, uses, nu);
        add_def(defs, nd, ins->dst);
        return;

    case LIR_STORE:
        operand_vreg_uses(ins->a, uses, nu);
        operand_vreg_uses(ins->b, uses, nu);
        return;

    case LIR_LEA:
        operand_vreg_uses(ins->a, uses, nu);
        add_def(defs, nd, ins->dst);
        return;

    case LIR_LEA_SYM:
        add_def(defs, nd, ins->dst);
        return;

    case LIR_ADD:
    case LIR_SUB:
    case LIR_MUL:
    case LIR_AND:
    case LIR_XOR:
    case LIR_OR:
    case LIR_SHL:
    case LIR_SHR:
    case LIR_SAR:
    case LIR_SETCC:
        operand_vreg_uses(ins->a, uses, nu);
        operand_vreg_uses(ins->b, uses, nu);
        add_def(defs, nd, ins->dst);
        if ((ins->op == LIR_SHL || ins->op == LIR_SHR || ins->op == LIR_SAR) &&
            ins->b.kind != OPND_IMM)
            block_phys(lv, td->shift_count_reg, i);
        return;

    case LIR_DIV:
    case LIR_MOD:
    case LIR_SDIV_POW2:
    case LIR_SMOD_POW2:
    case LIR_UDIV_POW2:
    case LIR_UMOD_POW2:
        operand_vreg_uses(ins->a, uses, nu);
        if (ins->op == LIR_DIV || ins->op == LIR_MOD)
            operand_vreg_uses(ins->b, uses, nu);
        add_def(defs, nd, ins->dst);
        if (ins->op == LIR_DIV || ins->op == LIR_MOD) {
            block_phys(lv, td->div_num_reg, i);
            block_phys(lv, td->div_rem_reg, i);
        }
        return;

    case LIR_BR:
        operand_vreg_uses(ins->a, uses, nu);
        operand_vreg_uses(ins->b, uses, nu);
        return;

    case LIR_CALL:
        if (ins->call_indirect)
            add_use(uses, nu, ins->call_reg);
        /* Arguments and an indirect callee are read at i.  The call clobbers
           caller-saved registers immediately afterward, at i + 1. */
        block_caller_saved(lv, td, i + 1);
        return;

    case LIR_RET:
        operand_vreg_uses(ins->a, uses, nu);
        operand_phys_touch(lv, ins->a, i);
        block_phys(lv, td->ret_reg, i);
        return;

    case LIR_MEMCPY:
        operand_vreg_uses(ins->a, uses, nu);
        operand_vreg_uses(ins->b, uses, nu);
        for (int r = 0; r < PHYS_COUNT; r++) {
            if (td->memcpy_clobber_mask & (1u << r))
                block_phys(lv, r, i);
        }
        return;
    }
}

static void bit_set(unsigned long *bits, int v)
{
    const int word_bits = (int)(sizeof(unsigned long) * CHAR_BIT);
    bits[v / word_bits] |= 1UL << (v % word_bits);
}

static int bit_test(const unsigned long *bits, int v)
{
    const int word_bits = (int)(sizeof(unsigned long) * CHAR_BIT);
    return (bits[v / word_bits] >> (v % word_bits)) & 1UL;
}

static void block_operand_uses(unsigned long *uses, const unsigned long *defs,
                               Operand operand)
{
    if (operand.kind == OPND_VREG) {
        if (!bit_test(defs, operand.u.vreg))
            bit_set(uses, operand.u.vreg);
        return;
    }
    if (operand.kind == OPND_MEM) {
        if (operand.u.mem.base >= 0 &&
            !bit_test(defs, operand.u.mem.base))
            bit_set(uses, operand.u.mem.base);
        if (operand.u.mem.index != LIR_NO_IDX &&
            !bit_test(defs, operand.u.mem.index))
            bit_set(uses, operand.u.mem.index);
    }
}

static int interval_cmp(const void *a, const void *b)
{
    const LiveInterval *ia = a;
    const LiveInterval *ib = b;
    if (ia->start != ib->start)
        return ia->start - ib->start;
    return ia->vreg - ib->vreg;
}

void liveness_compute(LirFn *lf, const TargetDesc *td, Liveness *out)
{
    const int word_bits = (int)(sizeof(unsigned long) * CHAR_BIT);
    int nwords;
    size_t matrix_words;
    unsigned long *use_bits;
    unsigned long *def_bits;
    unsigned long *live_in;
    unsigned long *live_out;

    memset(out, 0, sizeof(*out));

    if (lf->nvreg == 0)
        return;

    nwords = (int)(((size_t)lf->nvreg + (size_t)word_bits - 1) /
                   (size_t)word_bits);
    assert(nwords > 0);
    if ((size_t)lf->nblocks > SIZE_MAX / (size_t)nwords)
        diag_fatal("liveness data is too large");
    matrix_words = (size_t)lf->nblocks * (size_t)nwords;
    if (matrix_words > SIZE_MAX / sizeof(*use_bits))
        diag_fatal("liveness data is too large");
    use_bits = arena_alloc_zeroed(matrix_words * sizeof(*use_bits));
    def_bits = arena_alloc_zeroed(matrix_words * sizeof(*def_bits));
    live_in = arena_alloc_zeroed(matrix_words * sizeof(*live_in));
    live_out = arena_alloc_zeroed(matrix_words * sizeof(*live_out));

    for (int b = 0; b < lf->nblocks; b++) {
        LirBlock *block = &lf->blocks[b];
        unsigned long *ub = use_bits + (size_t)b * (size_t)nwords;
        unsigned long *db = def_bits + (size_t)b * (size_t)nwords;

        for (int i = 0; i < block->ninstr; i++) {
            Instr *ins = &block->instrs[i];
            int uses[MAX_REG_OPS], defs[MAX_REG_OPS];
            int nu, nd;

            instr_use_def(ins, ins->position, td, out,
                          uses, &nu, defs, &nd);
            for (int u = 0; u < nu; u++) {
                if (!bit_test(db, uses[u]))
                    bit_set(ub, uses[u]);
            }
            if (ins->op == LIR_CALL) {
                for (int a = 0; a < ins->nargs; a++)
                    block_operand_uses(ub, db, ins->call_args[a]);
            }
            for (int d = 0; d < nd; d++)
                bit_set(db, defs[d]);
        }

        if (block->term.kind == LIR_TERM_BR ||
            block->term.kind == LIR_TERM_RET) {
            Instr term = {0};
            int uses[MAX_REG_OPS], defs[MAX_REG_OPS];
            int nu, nd;

            term.op = block->term.kind == LIR_TERM_BR ? LIR_BR : LIR_RET;
            term.a = block->term.a;
            term.b = block->term.b;
            instr_use_def(&term, block->term.position, td, out,
                          uses, &nu, defs, &nd);
            for (int u = 0; u < nu; u++) {
                if (!bit_test(db, uses[u]))
                    bit_set(ub, uses[u]);
            }
        }
    }

    for (;;) {
        int changed = 0;

        for (int b = lf->nblocks - 1; b >= 0; b--) {
            LirBlock *block = &lf->blocks[b];
            unsigned long *in = live_in + (size_t)b * (size_t)nwords;
            unsigned long *out_bits = live_out + (size_t)b * (size_t)nwords;
            unsigned long *ub = use_bits + (size_t)b * (size_t)nwords;
            unsigned long *db = def_bits + (size_t)b * (size_t)nwords;

            for (int w = 0; w < nwords; w++) {
                unsigned long new_out = 0;
                unsigned long new_in;

                if (block->term.kind == LIR_TERM_JMP) {
                    new_out = live_in[
                        (size_t)block->term.target * (size_t)nwords + (size_t)w];
                } else if (block->term.kind == LIR_TERM_BR) {
                    new_out = live_in[
                        (size_t)block->term.true_target * (size_t)nwords + (size_t)w];
                    new_out |= live_in[
                        (size_t)block->term.false_target * (size_t)nwords + (size_t)w];
                }
                new_in = ub[w] | (new_out & ~db[w]);
                if (out_bits[w] != new_out || in[w] != new_in) {
                    out_bits[w] = new_out;
                    in[w] = new_in;
                    changed = 1;
                }
            }
        }
        if (!changed)
            break;
    }

    out->by_vreg = arena_alloc_zeroed((size_t)lf->nvreg * sizeof(*out->by_vreg));
    for (int v = 0; v < lf->nvreg; v++) {
        out->by_vreg[v].vreg = v;
        out->by_vreg[v].start = INT_MAX;
        out->by_vreg[v].end = -1;
    }
    for (int r = 0; r < PHYS_COUNT; r++)
        out->phys[r].npts = 0;

    for (int b = 0; b < lf->nblocks; b++) {
        LirBlock *block = &lf->blocks[b];
        unsigned long *in = live_in + (size_t)b * (size_t)nwords;
        unsigned long *out_bits = live_out + (size_t)b * (size_t)nwords;
        for (int v = 0; v < lf->nvreg; v++) {
            if (bit_test(in, v))
                touch_vreg_use(out, v, block->start_position);
            if (bit_test(out_bits, v))
                touch_vreg_use(out, v, block->end_position);
        }

        for (int i = 0; i < block->ninstr; i++) {
            Instr *ins = &block->instrs[i];
            int uses[MAX_REG_OPS], defs[MAX_REG_OPS];
            int nu, nd;

            instr_use_def(ins, ins->position, td, out,
                          uses, &nu, defs, &nd);
            for (int u = 0; u < nu; u++)
                touch_vreg_use(out, uses[u], ins->position);
            if (ins->op == LIR_CALL) {
                for (int a = 0; a < ins->nargs; a++) {
                    Operand arg = ins->call_args[a];
                    if (arg.kind == OPND_VREG)
                        touch_vreg_use(out, arg.u.vreg, ins->position);
                    else if (arg.kind == OPND_MEM) {
                        touch_vreg_use(out, arg.u.mem.base, ins->position);
                        if (arg.u.mem.index != LIR_NO_IDX)
                            touch_vreg_use(out, arg.u.mem.index, ins->position);
                    }
                }
            }
            for (int d = 0; d < nd; d++)
                touch_vreg_def(out, defs[d], ins->position);
        }

        if (block->term.kind == LIR_TERM_BR ||
            block->term.kind == LIR_TERM_RET) {
            Instr term = {0};
            int uses[MAX_REG_OPS], defs[MAX_REG_OPS];
            int nu, nd;

            term.op = block->term.kind == LIR_TERM_BR ? LIR_BR : LIR_RET;
            term.a = block->term.a;
            term.b = block->term.b;
            instr_use_def(&term, block->term.position, td, out,
                          uses, &nu, defs, &nd);
            for (int u = 0; u < nu; u++)
                touch_vreg_use(out, uses[u], block->term.position);
        }
    }

    for (int v = 0; v < lf->nvreg; v++) {
        LiveInterval *iv = &out->by_vreg[v];
        if (iv->end < 0 && iv->start != INT_MAX)
            iv->end = iv->start;
        if (iv->start == INT_MAX) {
            iv->start = 0;
            iv->end = -1;
        }
    }

    out->nsorted = 0;
    for (int v = 0; v < lf->nvreg; v++) {
        if (out->by_vreg[v].end >= 0)
            out->nsorted++;
    }

    out->sorted = arena_alloc((size_t)out->nsorted * sizeof(*out->sorted));
    int j = 0;
    for (int v = 0; v < lf->nvreg; v++) {
        if (out->by_vreg[v].end < 0)
            continue;
        out->sorted[j++] = out->by_vreg[v];
    }
    qsort(out->sorted, (size_t)out->nsorted, sizeof(*out->sorted), interval_cmp);
}

void liveness_dump(LirFn *lf, const Liveness *lv, const TargetDesc *td, FILE *out)
{
    fprintf(out, "function %s intervals (nvreg=%d)\n", lf->name, lf->nvreg);

    for (int i = 0; i < lv->nsorted; i++) {
        const LiveInterval *iv = &lv->sorted[i];
        fprintf(out, "  v%d [%d, %d]\n", iv->vreg, iv->start, iv->end);
    }

    fprintf(out, "  fixed blocks:\n");
    for (int r = 0; r < PHYS_COUNT; r++) {
        const PhysBlocks *pb = &lv->phys[r];
        if (pb->npts == 0)
            continue;
        fprintf(out, "    p%d:", r);
        for (int i = 0; i < pb->npts; i++)
            fprintf(out, " %d", pb->pts[i]);
        fputc('\n', out);
    }

    (void)td;
}
