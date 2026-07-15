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

static void instr_use_def(LirFn *lf, int i, const TargetDesc *td, Liveness *lv,
                          int *uses, int *nu, int *defs, int *nd)
{
    Instr *ins = &lf->instrs[i];

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
    case LIR_OR:
    case LIR_SHL:
    case LIR_SHR:
    case LIR_SAR:
    case LIR_SETCC:
        operand_vreg_uses(ins->a, uses, nu);
        operand_vreg_uses(ins->b, uses, nu);
        add_def(defs, nd, ins->dst);
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
        block_caller_saved(lv, td, i);
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

static void operand_bit_uses(unsigned long *bits, Operand o)
{
    switch (o.kind) {
    case OPND_VREG:
        bit_set(bits, o.u.vreg);
        return;
    case OPND_MEM:
        if (o.u.mem.base >= 0)
            bit_set(bits, o.u.mem.base);
        if (o.u.mem.index >= 0 && o.u.mem.index != LIR_NO_IDX)
            bit_set(bits, o.u.mem.index);
        return;
    default:
        return;
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
    int *label_instr;

    memset(out, 0, sizeof(*out));

    if (lf->nvreg == 0)
        return;

    nwords = (int)(((size_t)lf->nvreg + (size_t)word_bits - 1) /
                   (size_t)word_bits);
    assert(nwords > 0);
    if ((size_t)lf->ninstr > SIZE_MAX / (size_t)nwords)
        diag_fatal("liveness data is too large");
    matrix_words = (size_t)lf->ninstr * (size_t)nwords;
    if (matrix_words > SIZE_MAX / sizeof(*use_bits))
        diag_fatal("liveness data is too large");
    use_bits = arena_alloc_zeroed(matrix_words * sizeof(*use_bits));
    def_bits = arena_alloc_zeroed(matrix_words * sizeof(*def_bits));
    live_in = arena_alloc_zeroed(matrix_words * sizeof(*live_in));
    live_out = arena_alloc_zeroed(matrix_words * sizeof(*live_out));

    label_instr = arena_alloc((size_t)lf->label_count * sizeof(*label_instr));
    for (int label = 0; label < lf->label_count; label++)
        label_instr[label] = -1;
    for (int i = 0; i < lf->ninstr; i++) {
        Instr *ins = &lf->instrs[i];
        if (ins->op == LIR_LABEL) {
            assert(ins->label >= 0 && ins->label < lf->label_count);
            label_instr[ins->label] = i;
        }
    }

    for (int i = 0; i < lf->ninstr; i++) {
        int uses[MAX_REG_OPS];
        int defs[MAX_REG_OPS];
        int nu, nd;
        unsigned long *ub = use_bits + (size_t)i * (size_t)nwords;
        unsigned long *db = def_bits + (size_t)i * (size_t)nwords;

        instr_use_def(lf, i, td, out, uses, &nu, defs, &nd);
        for (int u = 0; u < nu; u++)
            bit_set(ub, uses[u]);
        if (lf->instrs[i].op == LIR_CALL) {
            for (int a = 0; a < lf->instrs[i].nargs; a++)
                operand_bit_uses(ub, lf->instrs[i].call_args[a]);
        }
        for (int d = 0; d < nd; d++)
            bit_set(db, defs[d]);
    }

    for (;;) {
        int changed = 0;

        for (int i = lf->ninstr - 1; i >= 0; i--) {
            Instr *ins = &lf->instrs[i];
            unsigned long *in = live_in + (size_t)i * (size_t)nwords;
            unsigned long *out_bits = live_out + (size_t)i * (size_t)nwords;
            unsigned long *ub = use_bits + (size_t)i * (size_t)nwords;
            unsigned long *db = def_bits + (size_t)i * (size_t)nwords;
            int target = -1;

            if (ins->op == LIR_JMP || ins->op == LIR_BR) {
                assert(ins->label >= 0 && ins->label < lf->label_count);
                target = label_instr[ins->label];
                assert(target >= 0);
            }

            for (int w = 0; w < nwords; w++) {
                unsigned long new_out = 0;
                unsigned long new_in;

                if (ins->op == LIR_JMP) {
                    new_out = live_in[(size_t)target * (size_t)nwords + (size_t)w];
                } else if (ins->op != LIR_RET) {
                    if (i + 1 < lf->ninstr)
                        new_out = live_in[(size_t)(i + 1) * (size_t)nwords + (size_t)w];
                    if (ins->op == LIR_BR)
                        new_out |= live_in[(size_t)target * (size_t)nwords + (size_t)w];
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

    for (int i = 0; i < lf->ninstr; i++) {
        unsigned long *ub = use_bits + (size_t)i * (size_t)nwords;
        unsigned long *db = def_bits + (size_t)i * (size_t)nwords;
        unsigned long *in = live_in + (size_t)i * (size_t)nwords;
        unsigned long *out_bits = live_out + (size_t)i * (size_t)nwords;

        for (int v = 0; v < lf->nvreg; v++) {
            if (!bit_test(ub, v) && !bit_test(db, v) &&
                !bit_test(in, v) && !bit_test(out_bits, v))
                continue;
            touch_vreg_def(out, v, i);
            touch_vreg_use(out, v, i);
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
