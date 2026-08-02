/* SPDX-License-Identifier: MIT */
#include "liveness.h"
#include "arena.h"
#include "diag.h"
#include "lir_dom.h"

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

static void record_vreg_position(Liveness *lv, int v, int idx, unsigned kind)
{
    LiveInterval *iv;

    if (v < 0)
        return;
    iv = &lv->by_vreg[v];
    for (int i = iv->npositions - 1; i >= 0; i--) {
        if (iv->positions[i].position == idx) {
            iv->positions[i].kind |= kind;
            return;
        }
        if (iv->positions[i].position < idx)
            break;
    }
    if (iv->npositions >= iv->positions_cap) {
        int new_cap = iv->positions_cap ? iv->positions_cap * 2 : 4;
        LivePosition *positions =
            arena_alloc((size_t)new_cap * sizeof(*positions));

        if (iv->positions)
            memcpy(positions, iv->positions,
                   (size_t)iv->npositions * sizeof(*positions));
        iv->positions = positions;
        iv->positions_cap = new_cap;
    }
    iv->positions[iv->npositions].position = idx;
    iv->positions[iv->npositions].kind = kind;
    iv->positions[iv->npositions].weight = 0;
    iv->npositions++;
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

static void block_callee_saved(Liveness *lv, const TargetDesc *td, int idx)
{
    for (int r = 0; r < PHYS_COUNT; r++) {
        if (td->callee_saved_mask & (1u << r))
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

static void block_incoming_arg_until(Liveness *lv, const TargetDesc *td,
                                     int phys, int idx)
{
    for (int a = 0; a < td->nargs_reg; a++) {
        if (td->arg_regs[a] != phys)
            continue;
        /* The ABI value already occupies this register on function entry.
           A virtual interval ending before its capture MOV must not be allowed
           to overwrite it.  Numbering uses even instruction positions, but
           cover odd clobber/split positions as well. */
        for (int p = 0; p <= idx; p++)
            block_phys(lv, phys, p);
        return;
    }
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
    case LIR_FMOVI:
        add_def(defs, nd, ins->dst);
        return;

    case LIR_MOV:
        operand_vreg_uses(ins->a, uses, nu);
        operand_phys_touch(lv, ins->a, i);
        if (ins->a.kind == OPND_PHYS)
            block_incoming_arg_until(lv, td, ins->a.u.phys, i);
        if (ins->dst >= 0) {
            add_def(defs, nd, ins->dst);
            return;
        }
        operand_phys_touch(lv, ins->b, i);
        return;

    case LIR_LOAD:
    case LIR_NEG:
    case LIR_FNEG:
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
    case LIR_FADD:
    case LIR_FSUB:
    case LIR_FMUL:
    case LIR_FDIV:
    case LIR_FSETCC:
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
        for (int a = 0; a < ins->nargs; a++)
            operand_vreg_uses(ins->call_args[a], uses, nu);
        /* Arguments and an indirect callee are read at i.  The call clobbers
           caller-saved registers immediately afterward, at i + 1. */
        block_caller_saved(lv, td, i + 1);
        /* setjmp hack: block callee-saved registers so locals spanning these
           libc calls are not allocated into registers that longjmp restores.
           See lir_call_is_setjmp_family() and include/README.md. */
        if (lir_call_is_setjmp_family(ins)) {
            block_callee_saved(lv, td, i);
            block_callee_saved(lv, td, i + 1);
        }
        if (ins->call_ret_type == LIR_TYPE_F80)
            add_def(defs, nd, ins->dst);
        return;

    case LIR_FRET:
        operand_vreg_uses(ins->a, uses, nu);
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

static int position_cmp(const void *a, const void *b)
{
    const LivePosition *pa = a;
    const LivePosition *pb = b;

    return pa->position - pb->position;
}

static int range_cmp(const void *a, const void *b)
{
    const LiveRange *ra = a;
    const LiveRange *rb = b;

    if (ra->start != rb->start)
        return ra->start - rb->start;
    return ra->end - rb->end;
}

static void add_live_range(LiveInterval *iv, int start, int end)
{
    if (start > end)
        return;
    if (iv->nranges >= iv->ranges_cap) {
        int new_cap = iv->ranges_cap ? iv->ranges_cap * 2 : 4;
        LiveRange *ranges = arena_alloc((size_t)new_cap * sizeof(*ranges));

        if (iv->ranges)
            memcpy(ranges, iv->ranges,
                   (size_t)iv->nranges * sizeof(*ranges));
        iv->ranges = ranges;
        iv->ranges_cap = new_cap;
    }
    iv->ranges[iv->nranges].start = start;
    iv->ranges[iv->nranges].end = end;
    iv->nranges++;
}

static void normalize_live_ranges(LiveInterval *iv)
{
    int out = 0;

    if (iv->nranges > 1)
        qsort(iv->ranges, (size_t)iv->nranges,
              sizeof(*iv->ranges), range_cmp);
    for (int i = 0; i < iv->nranges; i++) {
        LiveRange range = iv->ranges[i];

        if (out > 0 && range.start <= iv->ranges[out - 1].end + 1) {
            if (iv->ranges[out - 1].end < range.end)
                iv->ranges[out - 1].end = range.end;
        } else {
            iv->ranges[out++] = range;
        }
    }
    iv->nranges = out;
}

static int block_has_successor(const LirBlock *block, int successor)
{
    if (block->term.kind == LIR_TERM_JMP)
        return block->term.target == successor;
    if (block->term.kind == LIR_TERM_BR)
        return block->term.true_target == successor ||
               block->term.false_target == successor;
    return 0;
}

static void compute_loop_depths(const LirFn *lf, Liveness *out)
{
    LirDom dom;
    unsigned char *members;
    int *stack;

    out->block_loop_depth =
        arena_alloc_zeroed((size_t)lf->nblocks *
                           sizeof(*out->block_loop_depth));
    lir_dom_compute(lf, &dom);
    members = arena_alloc_zeroed((size_t)lf->nblocks);
    stack = arena_alloc((size_t)lf->nblocks * sizeof(*stack));

    for (int header = 0; header < lf->nblocks; header++) {
        int depth = 0;
        int found = 0;

        memset(members, 0, (size_t)lf->nblocks);
        for (int latch = 0; latch < lf->nblocks; latch++) {
            int nstack = 0;

            if (!block_has_successor(&lf->blocks[latch], header) ||
                !lir_dom_dominates(&dom, header, latch))
                continue;
            found = 1;
            members[header] = 1;
            if (!members[latch]) {
                members[latch] = 1;
                stack[nstack++] = latch;
            }
            while (nstack > 0) {
                int block = stack[--nstack];
                const LirBlock *bb = &lf->blocks[block];

                if (block == header)
                    continue;
                for (int p = 0; p < bb->npreds; p++) {
                    int pred = bb->preds[p];

                    if (!members[pred]) {
                        members[pred] = 1;
                        stack[nstack++] = pred;
                    }
                }
            }
        }
        if (!found)
            continue;
        for (int b = 0; b < lf->nblocks; b++)
            depth += members[b] != 0;
        if (depth == 0)
            continue;
        for (int b = 0; b < lf->nblocks; b++)
            out->block_loop_depth[b] += members[b] != 0;
    }
}

static int position_loop_depth(const LirFn *lf, const Liveness *lv,
                               int position)
{
    for (int b = 0; b < lf->nblocks; b++) {
        const LirBlock *block = &lf->blocks[b];

        if (position >= block->start_position &&
            position <= block->end_position)
            return lv->block_loop_depth[b];
    }
    return 0;
}

static unsigned position_weight(int loop_depth)
{
    unsigned weight = 1;

    while (loop_depth-- > 0 && weight < 1000)
        weight *= 10;
    return weight;
}

static void compute_spill_weights(const LirFn *lf, Liveness *out)
{
    for (int v = 0; v < lf->nvreg; v++) {
        LiveInterval *iv = &out->by_vreg[v];
        unsigned weight = 0;

        for (int p = 0; p < iv->npositions; p++) {
            LivePosition *position = &iv->positions[p];
            unsigned scale = position_weight(
                position_loop_depth(lf, out, position->position));
            unsigned contribution = 0;

            if (position->kind & LIVE_POS_USE)
                contribution += 4;
            if (position->kind & LIVE_POS_DEF)
                contribution += 1;
            if (contribution > (unsigned)INT_MAX / scale ||
                weight > (unsigned)INT_MAX - contribution * scale) {
                position->weight = INT_MAX;
                weight = INT_MAX;
                break;
            }
            position->weight = contribution * scale;
            weight += position->weight;
        }
        iv->spill_weight = weight;
    }
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
    out->fn = lf;

    if (lf->nvreg == 0)
        return;

    compute_loop_depths(lf, out);

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
            for (int u = 0; u < nu; u++) {
                touch_vreg_use(out, uses[u], ins->position);
                record_vreg_position(out, uses[u], ins->position, LIVE_POS_USE);
            }
            if (ins->op == LIR_CALL) {
                for (int a = 0; a < ins->nargs; a++) {
                    Operand arg = ins->call_args[a];
                    if (arg.kind == OPND_VREG) {
                        touch_vreg_use(out, arg.u.vreg, ins->position);
                        record_vreg_position(out, arg.u.vreg, ins->position,
                                             LIVE_POS_USE);
                    } else if (arg.kind == OPND_MEM) {
                        touch_vreg_use(out, arg.u.mem.base, ins->position);
                        record_vreg_position(out, arg.u.mem.base, ins->position,
                                             LIVE_POS_USE);
                        if (arg.u.mem.index != LIR_NO_IDX) {
                            touch_vreg_use(out, arg.u.mem.index, ins->position);
                            record_vreg_position(out, arg.u.mem.index,
                                                 ins->position, LIVE_POS_USE);
                        }
                    }
                }
            }
            for (int d = 0; d < nd; d++) {
                touch_vreg_def(out, defs[d], ins->position);
                record_vreg_position(out, defs[d], ins->position, LIVE_POS_DEF);
            }
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
                touch_vreg_use(out, uses[u], block->term.position);
                record_vreg_position(out, uses[u], block->term.position,
                                     LIVE_POS_USE);
            }
        }
    }

    /* Build block-precise ranges.  The legacy start/end pair remains the
       enclosing interval until allocation becomes range-aware. */
    for (int b = 0; b < lf->nblocks; b++) {
        LirBlock *block = &lf->blocks[b];
        unsigned long *in = live_in + (size_t)b * (size_t)nwords;
        unsigned long *out_bits = live_out + (size_t)b * (size_t)nwords;

        for (int v = 0; v < lf->nvreg; v++) {
            LiveInterval *iv = &out->by_vreg[v];
            int start = bit_test(in, v) ? block->start_position : INT_MAX;
            int end = bit_test(out_bits, v) ? block->end_position : -1;

            for (int p = 0; p < iv->npositions; p++) {
                int position = iv->positions[p].position;

                if (position < block->start_position ||
                    position > block->end_position)
                    continue;
                if (start > position)
                    start = position;
                if (end < position)
                    end = position;
            }
            if (start != INT_MAX || end >= 0) {
                if (start == INT_MAX)
                    start = end;
                if (end < 0)
                    end = start;
                add_live_range(iv, start, end);
            }
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
        if (iv->npositions > 1)
            qsort(iv->positions, (size_t)iv->npositions,
                  sizeof(*iv->positions), position_cmp);
        normalize_live_ranges(iv);
    }
    compute_spill_weights(lf, out);

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
        fprintf(out, "  v%d [%d, %d] weight=%u ranges(%d):",
                iv->vreg, iv->start, iv->end, iv->spill_weight,
                iv->nranges);
        for (int r = 0; r < iv->nranges; r++)
            fprintf(out, " [%d,%d]", iv->ranges[r].start, iv->ranges[r].end);
        fprintf(out, " positions(%d):", iv->npositions);
        for (int p = 0; p < iv->npositions; p++) {
            const LivePosition *pos = &iv->positions[p];
            fprintf(out, " %d", pos->position);
            if (pos->kind & LIVE_POS_USE)
                fputc('u', out);
            if (pos->kind & LIVE_POS_DEF)
                fputc('d', out);
            fprintf(out, "@%u", pos->weight);
        }
        fputc('\n', out);
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
