/* SPDX-License-Identifier: MIT */
#include "regalloc.h"
#include "lir.h"
#include "arena.h"
#include "diag.h"

#include <limits.h>

typedef struct {
    int vreg;
    int end;
} Active;

typedef struct {
    int off;
    int last_end;
} SpillSlot;

static int spill_offset(const Function *fn, int slot)
{
    long bytes = (long)fn->locals_size + 8L * slot;

    if (slot <= 0 || bytes > INT_MAX)
        diag_fatal("stack frame is too large");
    return -(int)bytes;
}

static int assign_spill_slot(SpillSlot **slots, int *nslots, int *nspill,
                             Function *fn, int start, int end)
{
    for (int i = 0; i < *nslots; i++) {
        if ((*slots)[i].last_end < start) {
            (*slots)[i].last_end = end;
            return (*slots)[i].off;
        }
    }

    (*nspill)++;
    int off = spill_offset(fn, *nspill);

    SpillSlot *n = arena_alloc((size_t)(*nslots + 1) * sizeof(*n));
    for (int i = 0; i < *nslots; i++)
        n[i] = (*slots)[i];
    n[*nslots].off = off;
    n[*nslots].last_end = end;
    *slots = n;
    (*nslots)++;
    return off;
}

static int reg_blocked(const Liveness *lv, int phys, int start, int end)
{
    const PhysBlocks *pb = &lv->phys[phys];
    for (int i = 0; i < pb->npts; i++) {
        int p = pb->pts[i];
        if (p >= start && p <= end)
            return 1;
    }
    return 0;
}

static int interval_spans_call(const LirFn *lf, int start, int end)
{
    if (start < 0)
        start = 0;
    if (end >= lf->npositions)
        end = lf->npositions - 1;
    for (int b = 0; b < lf->nblocks; b++) {
        const LirBlock *block = &lf->blocks[b];
        for (int i = 0; i < block->ninstr; i++) {
            const Instr *ins = &block->instrs[i];
            int clobber = ins->position + 1;
            if (clobber >= start && clobber <= end &&
                ins->op == LIR_CALL)
                return 1;
        }
    }
    return 0;
}

static int reg_ok_for_interval(const TargetDesc *td, int phys, int cross_call)
{
    if (phys < 0)
        return 0;
    if (!cross_call)
        return 1;
    return !(td->caller_saved_mask & (1u << phys));
}

static unsigned all_alloc_mask(const TargetDesc *td)
{
    unsigned m = 0;
    for (int i = 0; i < td->nalloc; i++)
        m |= 1u << td->alloc_order[i];
    return m;
}

static int free_reg(const TargetDesc *td, const Liveness *lv, unsigned freemask,
                    int start, int end, int cross_call)
{
    for (int i = 0; i < td->nalloc; i++) {
        int r = td->alloc_order[i];
        if (!(freemask & (1u << r)))
            continue;
        if (!reg_ok_for_interval(td, r, cross_call))
            continue;
        if (reg_blocked(lv, r, start, end))
            continue;
        return r;
    }
    return REG_NONE;
}

static void active_remove(Active *active, int *nactive, int idx)
{
    for (int i = idx + 1; i < *nactive; i++)
        active[i - 1] = active[i];
    (*nactive)--;
}

static void active_insert(Active *active, int *nactive, int vreg, int end)
{
    int i = *nactive;
    while (i > 0 && active[i - 1].end > end) {
        active[i] = active[i - 1];
        i--;
    }
    active[i].vreg = vreg;
    active[i].end = end;
    (*nactive)++;
}

static int active_index(const Active *active, int nactive, int vreg)
{
    for (int i = 0; i < nactive; i++) {
        if (active[i].vreg == vreg)
            return i;
    }
    return -1;
}

static void expire(Active *active, int *nactive, int *reg, unsigned *freemask,
                   const Liveness *lv, int start)
{
    for (int j = 0; j < *nactive; j++) {
        int v = active[j].vreg;
        if (lv->by_vreg[v].end >= start)
            return;
        *freemask |= 1u << reg[v];
        active_remove(active, nactive, j);
        j--;
    }
}

static int pick_spill_victim(const Active *active, int nactive, const LirFn *lf,
                             const Liveness *lv)
{
    if (nactive == 0)
        return REG_NONE;

    int best = REG_NONE;
    int best_end = -1;
    for (int j = 0; j < nactive; j++) {
        int v = active[j].vreg;
        if (lir_vreg_precolor(lf, v) >= 0)
            continue;
        if (lir_is_home_vreg(lf, v))
            continue;
        int e = lv->by_vreg[v].end;
        if (e > best_end) {
            best_end = e;
            best = v;
        }
    }
    if (best != REG_NONE)
        return best;
    for (int j = nactive - 1; j >= 0; j--) {
        if (lir_vreg_precolor(lf, active[j].vreg) < 0)
            return active[j].vreg;
    }
    return REG_NONE;
}

static int align_frame_size(long raw)
{
    if (raw < 0 || raw > INT_MAX - 15)
        diag_fatal("stack frame is too large");
    return (int)((raw + 15) & ~15);
}

void regalloc_trivial(LirFn *lf, Function *fn, AllocResult *out)
{
    int max_out = lir_max_outgoing(lf);

    out->vreg_reg = arena_alloc((size_t)lf->nvreg * sizeof(*out->vreg_reg));
    out->vreg_off = arena_alloc((size_t)lf->nvreg * sizeof(*out->vreg_off));
    for (int i = 0; i < lf->nvreg; i++) {
        out->vreg_reg[i] = REG_NONE;
        out->vreg_off[i] = spill_offset(fn, i + 1);
    }

    out->used_callee_saved = 0;
    out->frame_size = align_frame_size(
        (long)fn->locals_size + 8L * lf->nvreg + max_out);
}

void regalloc_linear(LirFn *lf, Function *fn, const Liveness *lv,
                     const TargetDesc *td, AllocResult *out)
{
    int max_out = lir_max_outgoing(lf);
    int nv = lf->nvreg;

    if (nv == 0) {
        out->vreg_reg = NULL;
        out->vreg_off = NULL;
        out->used_callee_saved = 0;
        out->frame_size = align_frame_size((long)fn->locals_size + max_out);
        return;
    }

    int *reg = arena_alloc((size_t)nv * sizeof(*reg));
    int *stackloc = arena_alloc_zeroed((size_t)nv * sizeof(*stackloc));
    int nspill = 0;
    SpillSlot *slots = NULL;
    int nslots = 0;

    for (int i = 0; i < nv; i++)
        reg[i] = REG_NONE;

    Active active[64];
    int nactive = 0;
    unsigned freemask = all_alloc_mask(td);

    for (int si = 0; si < lv->nsorted; si++) {
        int v = lv->sorted[si].vreg;
        const LiveInterval *iv = &lv->by_vreg[v];
        int start = iv->start;
        int end = iv->end;
        int fixed = lir_vreg_precolor(lf, v);

        expire(active, &nactive, reg, &freemask, lv, start);

        if (fixed >= 0) {
            reg[v] = fixed;
            freemask &= ~(1u << fixed);
            active_insert(active, &nactive, v, end);
            continue;
        }

        int cross_call = interval_spans_call(lf, start, end);
        int r = free_reg(td, lv, freemask, start, end, cross_call);
        if (r == REG_NONE) {
            int spill_v = pick_spill_victim(active, nactive, lf, lv);
            int steal = 0;

            if (spill_v != REG_NONE && lv->by_vreg[spill_v].end > end) {
                if (!lir_is_home_vreg(lf, spill_v) || lir_is_home_vreg(lf, v)) {
                    if (reg_ok_for_interval(td, reg[spill_v], cross_call))
                        steal = 1;
                }
            }

            if (steal) {
                reg[v] = reg[spill_v];
                stackloc[spill_v] = assign_spill_slot(
                    &slots, &nslots, &nspill, fn,
                    lv->by_vreg[spill_v].start,
                    lv->by_vreg[spill_v].end);
                reg[spill_v] = REG_NONE;
                {
                    int idx = active_index(active, nactive, spill_v);
                    if (idx >= 0)
                        active_remove(active, &nactive, idx);
                }
                active_insert(active, &nactive, v, end);
            } else {
                stackloc[v] = assign_spill_slot(
                    &slots, &nslots, &nspill, fn, start, end);
            }
            continue;
        }

        reg[v] = r;
        freemask &= ~(1u << r);
        active_insert(active, &nactive, v, end);
    }

    unsigned used_callee = 0;
    for (int i = 0; i < nv; i++) {
        int r = reg[i];
        if (r >= 0 && (td->callee_saved_mask & (1u << r)))
            used_callee |= 1u << r;
    }

    out->vreg_reg = arena_alloc((size_t)nv * sizeof(*out->vreg_reg));
    out->vreg_off = arena_alloc((size_t)nv * sizeof(*out->vreg_off));
    for (int i = 0; i < nv; i++) {
        out->vreg_reg[i] = reg[i];
        out->vreg_off[i] = stackloc[i];
    }

    out->used_callee_saved = used_callee;
    out->frame_size = align_frame_size(
        (long)fn->locals_size + 8L * nspill + max_out);
}
