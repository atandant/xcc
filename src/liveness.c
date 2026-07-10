/* SPDX-License-Identifier: MIT */
#include "liveness.h"
#include "arena.h"

#include <limits.h>
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
    if (*nu >= MAX_REG_OPS)
        return;
    uses[(*nu)++] = v;
}

static void add_def(int *defs, int *nd, int v)
{
    if (v < 0)
        return;
    for (int i = 0; i < *nd; i++)
        if (defs[i] == v)
            return;
    if (*nd >= MAX_REG_OPS)
        return;
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

static void touch_operand_use(Liveness *lv, Operand o, int idx)
{
    switch (o.kind) {
    case OPND_VREG:
        touch_vreg_use(lv, o.u.vreg, idx);
        return;
    case OPND_MEM:
        if (o.u.mem.base >= 0)
            touch_vreg_use(lv, o.u.mem.base, idx);
        if (o.u.mem.index >= 0 && o.u.mem.index != LIR_NO_IDX)
            touch_vreg_use(lv, o.u.mem.index, idx);
        return;
    default:
        return;
    }
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
        operand_vreg_uses(ins->a, uses, nu);
        operand_vreg_uses(ins->b, uses, nu);
        add_def(defs, nd, ins->dst);
        block_phys(lv, td->div_num_reg, i);
        block_phys(lv, td->div_rem_reg, i);
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
        return;
    }
}

static int vreg_used_in_range(LirFn *lf, const TargetDesc *td, Liveness *lv,
                              int v, int b, int e)
{
    if (b < 0)
        b = 0;
    if (e >= lf->ninstr)
        e = lf->ninstr - 1;

    for (int i = b; i <= e; i++) {
        Instr *ins = &lf->instrs[i];

        if (ins->op == LIR_CALL) {
            for (int a = 0; a < ins->nargs; a++) {
                Operand o = ins->call_args[a];
                if (o.kind == OPND_VREG && o.u.vreg == v)
                    return 1;
                if (o.kind == OPND_MEM) {
                    if (o.u.mem.base == v)
                        return 1;
                    if (o.u.mem.index == v)
                        return 1;
                }
            }
            continue;
        }

        int uses[MAX_REG_OPS];
        int defs[MAX_REG_OPS];
        int nu, nd;

        instr_use_def(lf, i, td, lv, uses, &nu, defs, &nd);
        for (int j = 0; j < nu; j++) {
            if (uses[j] == v)
                return 1;
        }
    }
    return 0;
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
    memset(out, 0, sizeof(*out));

    if (lf->nvreg == 0)
        return;

    out->by_vreg = arena_alloc_zeroed((size_t)lf->nvreg * sizeof(*out->by_vreg));
    for (int v = 0; v < lf->nvreg; v++) {
        out->by_vreg[v].vreg = v;
        out->by_vreg[v].start = INT_MAX;
        out->by_vreg[v].end = -1;
    }

    for (int i = lf->ninstr - 1; i >= 0; i--) {
        Instr *ins = &lf->instrs[i];

        if (ins->op == LIR_CALL) {
            for (int a = 0; a < ins->nargs; a++)
                touch_operand_use(out, ins->call_args[a], i);
            block_caller_saved(out, td, i);
            continue;
        }

        int uses[MAX_REG_OPS];
        int defs[MAX_REG_OPS];
        int nu, nd;

        instr_use_def(lf, i, td, out, uses, &nu, defs, &nd);

        for (int d = 0; d < nd; d++)
            touch_vreg_def(out, defs[d], i);

        for (int u = 0; u < nu; u++)
            touch_vreg_use(out, uses[u], i);
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

    for (int l = 0; l < lf->nloops; l++) {
        int b = lf->loops[l].begin;
        int e = lf->loops[l].end;

        for (int v = 0; v < lf->nvreg; v++) {
            LiveInterval *iv = &out->by_vreg[v];

            if (iv->end < 0)
                continue;
            if (iv->start > e || iv->end < b)
                continue;
            if (!vreg_used_in_range(lf, td, out, v, b, e))
                continue;
            if (iv->end >= b && iv->end <= e)
                iv->end = e;
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

    for (int i = 0; i < lf->nloops; i++)
        fprintf(out, "  loop [%d, %d]\n", lf->loops[i].begin, lf->loops[i].end);

    (void)td;
}
