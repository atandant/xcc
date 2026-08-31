/* SPDX-License-Identifier: MIT */
#include "regalloc.h"
#include "lir.h"
#include "lir_cfg.h"
#include "arena.h"
#include "diag.h"

#include <limits.h>
#include <string.h>

typedef struct {
    int vreg;
    int end;
} Active;

typedef struct {
    int off;
    int last_end;
    int size;
    int align;
} SpillSlot;

static long align_up(long value, int align)
{
    return (value + align - 1) & -(long)align;
}

static int new_spill_offset(const Function *fn, int *spill_bytes,
                            int size, int align)
{
    long end = align_up((long)fn->locals_size + *spill_bytes + size,
                        align);

    if (end > INT_MAX)
        diag_fatal("stack frame is too large");
    *spill_bytes = (int)end - fn->locals_size;
    return -(int)end;
}

static int assign_spill_slot(SpillSlot **slots, int *nslots, int *nspill,
                             int *spill_bytes, Function *fn, LirType type,
                             int start, int end)
{
    int size = lir_type_storage_size(type);
    int align = lir_type_storage_align(type);

    for (int i = 0; i < *nslots; i++) {
        if ((*slots)[i].last_end < start && (*slots)[i].size == size &&
            (*slots)[i].align == align) {
            (*slots)[i].last_end = end;
            return (*slots)[i].off;
        }
    }

    (*nspill)++;
    int off = new_spill_offset(fn, spill_bytes, size, align);

    SpillSlot *n = arena_alloc((size_t)(*nslots + 1) * sizeof(*n));
    for (int i = 0; i < *nslots; i++)
        n[i] = (*slots)[i];
    n[*nslots].off = off;
    n[*nslots].last_end = end;
    n[*nslots].size = size;
    n[*nslots].align = align;
    *slots = n;
    (*nslots)++;
    return off;
}

static int interval_covers(const LiveInterval *iv, int position)
{
    for (int i = 0; i < iv->nranges; i++) {
        if (position < iv->ranges[i].start)
            return 0;
        if (position <= iv->ranges[i].end)
            return 1;
    }
    return 0;
}

static int intervals_overlap(const LiveInterval *a, const LiveInterval *b)
{
    int i = 0;
    int j = 0;

    while (i < a->nranges && j < b->nranges) {
        const LiveRange *ra = &a->ranges[i];
        const LiveRange *rb = &b->ranges[j];

        if (ra->end < rb->start)
            i++;
        else if (rb->end < ra->start)
            j++;
        else
            return 1;
    }
    return 0;
}

static int reg_blocked(const Liveness *lv, int phys,
                       const LiveInterval *iv)
{
    const PhysBlocks *pb = &lv->phys[phys];
    for (int i = 0; i < pb->npts; i++) {
        if (interval_covers(iv, pb->pts[i]))
            return 1;
    }
    return 0;
}

static int interval_spans_call(const LirFn *lf, const LiveInterval *iv)
{
    for (int b = 0; b < lf->nblocks; b++) {
        const LirBlock *block = &lf->blocks[b];
        for (int i = 0; i < block->ninstr; i++) {
            const Instr *ins = &block->instrs[i];
            int clobber = ins->position + 1;
            if (ins->op == LIR_CALL && interval_covers(iv, clobber))
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

static int phys_available(const Liveness *lv, const int *assigned, int nv,
                          int current, int phys, int ignore)
{
    const LiveInterval *iv = &lv->by_vreg[current];

    if (reg_blocked(lv, phys, iv))
        return 0;
    for (int v = 0; v < nv; v++) {
        if (v == ignore || assigned[v] != phys)
            continue;
        if (intervals_overlap(iv, &lv->by_vreg[v]))
            return 0;
    }
    return 1;
}

static int free_reg(const TargetDesc *td, const Liveness *lv,
                    const int *assigned, int nv, int current, int cross_call)
{
    RegClass reg_class = lir_vreg_class(lv->fn, current);

    for (int i = 0; i < td->nalloc[reg_class]; i++) {
        int r = td->alloc_order[reg_class][i];
        if (!reg_ok_for_interval(td, r, cross_call))
            continue;
        if (!phys_available(lv, assigned, nv, current, r, LIR_NO_VREG))
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

static void expire(Active *active, int *nactive, const Liveness *lv, int start)
{
    for (int j = 0; j < *nactive; j++) {
        int v = active[j].vreg;
        if (lv->by_vreg[v].end >= start)
            return;
        active_remove(active, nactive, j);
        j--;
    }
}

static unsigned remaining_spill_weight(const LiveInterval *iv, int position)
{
    unsigned weight = 0;

    for (int i = 0; i < iv->npositions; i++) {
        if (iv->positions[i].position < position)
            continue;
        if (weight > (unsigned)INT_MAX - iv->positions[i].weight)
            return INT_MAX;
        weight += iv->positions[i].weight;
    }
    return weight;
}

static int pick_spill_victim(const Active *active, int nactive, const LirFn *lf,
                             const Liveness *lv, int current, int position)
{
    if (nactive == 0)
        return REG_NONE;

    int best = REG_NONE;
    unsigned best_weight = INT_MAX;
    int best_end = -1;
    int current_home = lir_is_home_vreg(lf, current);

    for (int pass = 0; pass < 2 && best == REG_NONE; pass++) {
        for (int j = 0; j < nactive; j++) {
            int v = active[j].vreg;
            unsigned weight;
            int end;

            if (lir_vreg_class(lf, v) != lir_vreg_class(lf, current))
                continue;
            if (lir_vreg_precolor(lf, v) >= 0)
                continue;
            if (!current_home && lir_is_home_vreg(lf, v))
                continue;
            if (pass == 0 && lir_is_home_vreg(lf, v))
                continue;
            weight = remaining_spill_weight(&lv->by_vreg[v], position);
            end = lv->by_vreg[v].end;
            if (best == REG_NONE || weight < best_weight ||
                (weight == best_weight && end > best_end)) {
                best_weight = weight;
                best_end = end;
                best = v;
            }
        }
    }
    return best;
}

static int align_frame_size(long raw)
{
    if (raw < 0 || raw > INT_MAX - 15)
        diag_fatal("stack frame is too large");
    return (int)((raw + 15) & ~15);
}

static int mask_count(unsigned mask)
{
    int count = 0;

    while (mask) {
        count += mask & 1u;
        mask >>= 1;
    }
    return count;
}

/* Return the input that the target can consume destructively in dst.  When
   that input dies here, assigning it and dst the same register avoids the
   otherwise mandatory copy imposed by x86's two-address instructions. */
static int coalesce_source(const Instr *ins)
{
    if (ins->a.kind != OPND_VREG || ins->dst == LIR_NO_VREG)
        return LIR_NO_VREG;

    switch (ins->op) {
    case LIR_MOV:
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
    case LIR_FADD:
    case LIR_FSUB:
    case LIR_FMUL:
    case LIR_FDIV:
    case LIR_FNEG:
        return ins->a.u.vreg;
    default:
        return LIR_NO_VREG;
    }
}

static int is_register_affinity(const LirFn *lf, const Liveness *lv,
                                const Instr *ins, int *src_out)
{
    int src = coalesce_source(ins);

    if (src == LIR_NO_VREG ||
        lv->by_vreg[ins->dst].start != ins->position ||
        lv->by_vreg[src].end != ins->position ||
        lir_vreg_precolor(lf, ins->dst) >= 0 ||
        lir_vreg_precolor(lf, src) >= 0 ||
        lir_vreg_class(lf, ins->dst) != lir_vreg_class(lf, src))
        return 0;
    *src_out = src;
    return 1;
}

/* Improve two-address placement after linear scan, without perturbing the
   scan's spill decisions.  Restrict recoloring to values with one incoming
   affinity and no outgoing affinity, so satisfying this edge cannot break a
   different already-coalesced edge. */
static void improve_register_affinities(const LirFn *lf, const Liveness *lv,
                                        const TargetDesc *td, int *reg)
{
    int nv = lf->nvreg;
    int *incoming = arena_alloc_zeroed((size_t)nv * sizeof(*incoming));
    int *outgoing = arena_alloc_zeroed((size_t)nv * sizeof(*outgoing));

    for (int b = 0; b < lf->nblocks; b++) {
        const LirBlock *block = &lf->blocks[b];
        for (int i = 0; i < block->ninstr; i++) {
            const Instr *ins = &block->instrs[i];
            int src;

            if (!is_register_affinity(lf, lv, ins, &src))
                continue;
            incoming[ins->dst]++;
            outgoing[src]++;
        }
    }

    for (int b = 0; b < lf->nblocks; b++) {
        const LirBlock *block = &lf->blocks[b];
        for (int i = 0; i < block->ninstr; i++) {
            const Instr *ins = &block->instrs[i];
            int src;

            if (!is_register_affinity(lf, lv, ins, &src) ||
                incoming[ins->dst] != 1 || outgoing[ins->dst] != 0 ||
                reg[src] < 0 || reg[ins->dst] < 0 ||
                reg[src] == reg[ins->dst])
                continue;
            int preferred = reg[src];
            int cross_call = interval_spans_call(lf, &lv->by_vreg[ins->dst]);
            if (reg_ok_for_interval(td, preferred, cross_call) &&
                phys_available(lv, reg, nv, ins->dst, preferred, src))
                reg[ins->dst] = preferred;
        }
    }
}

void regalloc_trivial(LirFn *lf, Function *fn, AllocResult *out)
{
    int max_out = lir_max_outgoing(lf);
    int spill_bytes = 0;

    out->vreg_reg = arena_alloc((size_t)lf->nvreg * sizeof(*out->vreg_reg));
    out->vreg_off = arena_alloc((size_t)lf->nvreg * sizeof(*out->vreg_off));
    for (int i = 0; i < lf->nvreg; i++) {
        out->vreg_reg[i] = REG_NONE;
        out->vreg_off[i] = new_spill_offset(fn, &spill_bytes,
            lir_type_storage_size(lir_vreg_type(lf, i)),
            lir_type_storage_align(lir_vreg_type(lf, i)));
    }

    out->used_callee_saved = 0;
    out->fragment_parent = NULL;
    out->live_vregs = lf->nvreg;
    out->register_vregs = 0;
    out->spilled_vregs = lf->nvreg;
    out->spill_slots = lf->nvreg;
    out->range_reuses = 0;
    out->split_vregs = 0;
    out->split_fragments = 0;
    out->split_moves = 0;
    out->outgoing_size = max_out;
    out->frame_size = align_frame_size(
        (long)fn->locals_size + spill_bytes + max_out);
}

static void regalloc_pass(LirFn *lf, Function *fn, const Liveness *lv,
                          const TargetDesc *td,
                          const unsigned char *force_stack, AllocResult *out)
{
    int max_out = lir_max_outgoing(lf);
    int nv = lf->nvreg;

    if (nv == 0) {
        out->vreg_reg = NULL;
        out->vreg_off = NULL;
        out->fragment_parent = NULL;
        out->used_callee_saved = 0;
        out->live_vregs = 0;
        out->register_vregs = 0;
        out->spilled_vregs = 0;
        out->spill_slots = 0;
        out->range_reuses = 0;
        out->split_vregs = 0;
        out->split_fragments = 0;
        out->split_moves = 0;
        out->outgoing_size = max_out;
        out->frame_size = align_frame_size((long)fn->locals_size + max_out);
        return;
    }

    int *reg = arena_alloc((size_t)nv * sizeof(*reg));
    int *stackloc = arena_alloc_zeroed((size_t)nv * sizeof(*stackloc));
    int *move_src = arena_alloc((size_t)nv * sizeof(*move_src));
    int nspill = 0;
    int spill_bytes = 0;
    SpillSlot *slots = NULL;
    int nslots = 0;

    for (int i = 0; i < nv; i++) {
        reg[i] = REG_NONE;
        move_src[i] = LIR_NO_VREG;
    }
    for (int b = 0; b < lf->nblocks; b++) {
        const LirBlock *block = &lf->blocks[b];
        for (int i = 0; i < block->ninstr; i++) {
            const Instr *ins = &block->instrs[i];
            if (ins->op == LIR_MOV && ins->dst != LIR_NO_VREG &&
                ins->a.kind == OPND_VREG &&
                lv->by_vreg[ins->dst].start == ins->position &&
                lv->by_vreg[ins->a.u.vreg].end == ins->position)
                move_src[ins->dst] = ins->a.u.vreg;
        }
    }

    Active *active = arena_alloc((size_t)nv * sizeof(*active));
    int nactive = 0;

    for (int si = 0; si < lv->nsorted; si++) {
        int v = lv->sorted[si].vreg;
        const LiveInterval *iv = &lv->by_vreg[v];
        int start = iv->start;
        int end = iv->end;
        int fixed = lir_vreg_precolor(lf, v);

        expire(active, &nactive, lv, start);

        if (force_stack && force_stack[v]) {
            stackloc[v] = assign_spill_slot(
                &slots, &nslots, &nspill, &spill_bytes, fn,
                lir_vreg_type(lf, v), start, end);
            continue;
        }

        if (lir_vreg_class(lf, v) == REG_CLASS_MEMORY) {
            stackloc[v] = assign_spill_slot(
                &slots, &nslots, &nspill, &spill_bytes, fn,
                lir_vreg_type(lf, v), start, end);
            continue;
        }

        if (fixed >= 0) {
            reg[v] = fixed;
            active_insert(active, &nactive, v, end);
            continue;
        }

        int cross_call = interval_spans_call(lf, iv);
        int r = REG_NONE;
        int src = move_src[v];
        if (src != LIR_NO_VREG && reg[src] >= 0 &&
            lir_vreg_class(lf, src) == lir_vreg_class(lf, v)) {
            int preferred = reg[src];
            if (reg_ok_for_interval(td, preferred, cross_call) &&
                phys_available(lv, reg, nv, v, preferred, src)) {
                r = preferred;
            }
        }
        if (r == REG_NONE)
            r = free_reg(td, lv, reg, nv, v, cross_call);
        if (r == REG_NONE) {
            int spill_v = pick_spill_victim(active, nactive, lf, lv, v, start);
            int steal = 0;

            if (spill_v != REG_NONE) {
                unsigned victim_weight = remaining_spill_weight(
                    &lv->by_vreg[spill_v], start);
                unsigned current_weight = remaining_spill_weight(iv, start);

                if ((victim_weight < current_weight ||
                     (victim_weight == current_weight &&
                      lv->by_vreg[spill_v].end > end)) &&
                    reg_ok_for_interval(td, reg[spill_v], cross_call) &&
                    phys_available(lv, reg, nv, v, reg[spill_v], spill_v))
                    steal = 1;
            }

            if (steal) {
                reg[v] = reg[spill_v];
                stackloc[spill_v] = assign_spill_slot(
                    &slots, &nslots, &nspill, &spill_bytes, fn,
                    lir_vreg_type(lf, spill_v),
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
                    &slots, &nslots, &nspill, &spill_bytes, fn,
                    lir_vreg_type(lf, v), start, end);
            }
            continue;
        }

        reg[v] = r;
        active_insert(active, &nactive, v, end);
    }

    improve_register_affinities(lf, lv, td, reg);

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
    out->fragment_parent = NULL;
    out->live_vregs = lv->nsorted;
    out->register_vregs = 0;
    for (int i = 0; i < nv; i++) {
        if (lv->by_vreg[i].end >= 0 && reg[i] >= 0)
            out->register_vregs++;
    }
    out->spilled_vregs = out->live_vregs - out->register_vregs;
    out->spill_slots = nspill;
    out->range_reuses = 0;
    for (int v = 0; v < nv; v++) {
        if (reg[v] < 0)
            continue;
        for (int u = 0; u < v; u++) {
            if (reg[u] == reg[v] &&
                !intervals_overlap(&lv->by_vreg[u], &lv->by_vreg[v])) {
                out->range_reuses++;
                break;
            }
        }
    }
    out->outgoing_size = max_out;
    out->split_vregs = 0;
    out->split_fragments = 0;
    out->split_moves = 0;
    out->frame_size = align_frame_size(
        (long)fn->locals_size + spill_bytes + max_out);
}

static int operand_mentions_vreg(Operand op, int vreg)
{
    if (op.kind == OPND_VREG)
        return op.u.vreg == vreg;
    if (op.kind != OPND_MEM)
        return 0;
    return op.u.mem.base == vreg || op.u.mem.index == vreg;
}

static int instr_mentions_vreg(const Instr *ins, int vreg)
{
    if ((lir_instruction_defines_vreg(ins) && ins->dst == vreg) ||
        (ins->op == LIR_CALL && ins->call_indirect && ins->call_reg == vreg) ||
        operand_mentions_vreg(ins->a, vreg) ||
        operand_mentions_vreg(ins->b, vreg))
        return 1;
    for (int i = 0; i < ins->nargs; i++) {
        if (operand_mentions_vreg(ins->call_args[i], vreg))
            return 1;
    }
    return 0;
}

static int instr_uses_vreg(const Instr *ins, int vreg)
{
    if ((ins->op == LIR_CALL && ins->call_indirect &&
         ins->call_reg == vreg) ||
        operand_mentions_vreg(ins->a, vreg) ||
        operand_mentions_vreg(ins->b, vreg))
        return 1;
    for (int i = 0; i < ins->nargs; i++) {
        if (operand_mentions_vreg(ins->call_args[i], vreg))
            return 1;
    }
    return 0;
}

static int block_mentions_vreg(const LirBlock *block, int vreg)
{
    for (int i = 0; i < block->ninstr; i++) {
        if (instr_mentions_vreg(&block->instrs[i], vreg))
            return 1;
    }
    return operand_mentions_vreg(block->term.a, vreg) ||
           operand_mentions_vreg(block->term.b, vreg);
}

static void replace_operand_vreg(Operand *op, int from, int to)
{
    if (op->kind == OPND_VREG) {
        if (op->u.vreg == from)
            op->u.vreg = to;
        return;
    }
    if (op->kind != OPND_MEM)
        return;
    if (op->u.mem.base == from)
        op->u.mem.base = to;
    if (op->u.mem.index == from)
        op->u.mem.index = to;
}

static void replace_instr_vreg(Instr *ins, int from, int to)
{
    if (lir_instruction_defines_vreg(ins) && ins->dst == from)
        ins->dst = to;
    if (ins->op == LIR_CALL && ins->call_indirect && ins->call_reg == from)
        ins->call_reg = to;
    replace_operand_vreg(&ins->a, from, to);
    replace_operand_vreg(&ins->b, from, to);
    for (int i = 0; i < ins->nargs; i++)
        replace_operand_vreg(&ins->call_args[i], from, to);
}

static Instr fragment_move(int dst, int src)
{
    Instr ins = {0};

    ins.op = LIR_MOV;
    ins.dst = dst;
    ins.a = lir_vreg(src);
    ins.b = lir_none();
    ins.w = LIR_W8;
    return ins;
}

static void append_instr(Instr **instrs, int *ninstr, int *cap, Instr ins)
{
    if (*ninstr >= *cap) {
        int new_cap = *cap ? *cap * 2 : 16;
        Instr *new_instrs = arena_alloc((size_t)new_cap * sizeof(*new_instrs));

        if (*instrs)
            memcpy(new_instrs, *instrs,
                   (size_t)*ninstr * sizeof(*new_instrs));
        *instrs = new_instrs;
        *cap = new_cap;
    }
    (*instrs)[(*ninstr)++] = ins;
}

/* Turn each wholly spilled vreg into a stack-resident parent plus ordinary
   fragments for consecutive uses/definitions inside a block.  The parent is
   the canonical value between fragments and on CFG edges, so joins and
   critical edges need no path-specific repair copies.  Keeping consecutive
   touches together avoids reloads within expression chains while still
   shortening fragments enough to relieve local register pressure. */
static int materialize_spill_fragments(LirFn *lf, const Liveness *lv,
                                       const AllocResult *first,
                                       unsigned char **force_stack_out,
                                       int **parents_out, int *split_vregs,
                                       int *split_fragments, int *split_moves)
{
    int original_nv = lf->nvreg;
    unsigned char *split = arena_alloc_zeroed((size_t)original_nv);
    int any = 0;

    for (int v = 0; v < original_nv; v++) {
        if (lv->by_vreg[v].end >= 0 && first->vreg_reg[v] < 0 &&
            lir_vreg_precolor(lf, v) < 0 &&
            lir_vreg_class(lf, v) != REG_CLASS_MEMORY) {
            split[v] = 1;
            any = 1;
            (*split_vregs)++;
        }
    }
    if (!any)
        return 0;

    int max_fragments = 0;
    for (int b = 0; b < lf->nblocks; b++) {
        const LirBlock *block = &lf->blocks[b];

        for (int v = 0; v < original_nv; v++) {
            if (!split[v])
                continue;
            for (int i = 0; i < block->ninstr; i++)
                max_fragments += instr_mentions_vreg(&block->instrs[i], v);
            max_fragments += operand_mentions_vreg(block->term.a, v) ||
                             operand_mentions_vreg(block->term.b, v);
        }
    }
    int final_cap = original_nv + max_fragments;
    unsigned char *force_stack = arena_alloc_zeroed((size_t)final_cap);
    int *parents = arena_alloc((size_t)final_cap * sizeof(*parents));
    for (int v = 0; v < final_cap; v++)
        parents[v] = LIR_NO_VREG;
    for (int v = 0; v < original_nv; v++) {
        if (split[v])
            force_stack[v] = 1;
    }

    for (int v = 0; v < original_nv; v++) {
        if (!split[v])
            continue;
        for (int b = 0; b < lf->nblocks; b++) {
            LirBlock *block = &lf->blocks[b];
            Instr *new_instrs = NULL;
            int new_ninstr = 0;
            int new_cap = 0;
            int fragment = LIR_NO_VREG;
            int fragment_defined = 0;
            int previous_touched = 0;

            if (!block_mentions_vreg(block, v))
                continue;
            for (int i = 0; i < block->ninstr; i++) {
                Instr ins = block->instrs[i];
                int touched = instr_mentions_vreg(&ins, v);

                if (touched && !previous_touched) {
                    fragment = lir_new_vreg_type(lf, lir_vreg_type(lf, v));
                    parents[fragment] = v;
                    (*split_fragments)++;
                    if (instr_uses_vreg(&ins, v)) {
                        append_instr(&new_instrs, &new_ninstr, &new_cap,
                                     fragment_move(fragment, v));
                        (*split_moves)++;
                    }
                }
                if (touched) {
                    if (ins.nargs > 0) {
                        Operand *args = arena_alloc(
                            (size_t)ins.nargs * sizeof(*args));
                        memcpy(args, ins.call_args,
                               (size_t)ins.nargs * sizeof(*args));
                        ins.call_args = args;
                    }
                    fragment_defined |=
                        lir_instruction_defines_vreg(&ins) && ins.dst == v;
                    replace_instr_vreg(&ins, v, fragment);
                }
                append_instr(&new_instrs, &new_ninstr, &new_cap, ins);

                if (previous_touched && !touched && fragment_defined) {
                    /* Insert before the first unrelated instruction. */
                    Instr unrelated = new_instrs[--new_ninstr];
                    append_instr(&new_instrs, &new_ninstr, &new_cap,
                                 fragment_move(v, fragment));
                    append_instr(&new_instrs, &new_ninstr, &new_cap,
                                 unrelated);
                    (*split_moves)++;
                    fragment_defined = 0;
                }
                previous_touched = touched;
            }

            int term_touched = operand_mentions_vreg(block->term.a, v) ||
                               operand_mentions_vreg(block->term.b, v);
            if (term_touched && !previous_touched) {
                fragment = lir_new_vreg_type(lf, lir_vreg_type(lf, v));
                parents[fragment] = v;
                (*split_fragments)++;
                append_instr(&new_instrs, &new_ninstr, &new_cap,
                             fragment_move(fragment, v));
                (*split_moves)++;
            }
            if (term_touched) {
                replace_operand_vreg(&block->term.a, v, fragment);
                replace_operand_vreg(&block->term.b, v, fragment);
            }
            if (fragment_defined) {
                append_instr(&new_instrs, &new_ninstr, &new_cap,
                             fragment_move(v, fragment));
                (*split_moves)++;
            }
            block->instrs = new_instrs;
            block->ninstr = new_ninstr;
            block->cap = new_cap;
        }
    }

    *force_stack_out = force_stack;
    *parents_out = parents;
    return 1;
}

void regalloc_linear(LirFn *lf, Function *fn, Liveness *lv,
                     const TargetDesc *td, AllocResult *out)
{

    AllocResult first;
    unsigned char *force_stack = NULL;
    int *parents = NULL;
    int split_vregs = 0;
    int split_fragments = 0;
    int split_moves = 0;

    regalloc_pass(lf, fn, lv, td, NULL, &first);
    if (!materialize_spill_fragments(lf, lv, &first, &force_stack, &parents,
                                     &split_vregs, &split_fragments,
                                     &split_moves)) {
        *out = first;
        return;
    }

    lir_cfg_number_instructions(lf);
    liveness_compute(lf, td, lv);
    regalloc_pass(lf, fn, lv, td, force_stack, out);
    out->fragment_parent = parents;
    out->split_vregs = split_vregs;
    out->split_fragments = split_fragments;
    out->split_moves = split_moves;
}

static int single_overlap_position(const LiveInterval *a,
                                   const LiveInterval *b, int *position)
{
    int found = 0;

    for (int i = 0; i < a->nranges; i++) {
        for (int j = 0; j < b->nranges; j++) {
            int start = a->ranges[i].start > b->ranges[j].start
                ? a->ranges[i].start : b->ranges[j].start;
            int end = a->ranges[i].end < b->ranges[j].end
                ? a->ranges[i].end : b->ranges[j].end;

            if (start > end)
                continue;
            if (start != end || (found && *position != start))
                return 0;
            *position = start;
            found = 1;
        }
    }
    return found;
}

static int coalescing_instruction_at(const LirFn *lf, int a, int b,
                                     int position)
{
    for (int block = 0; block < lf->nblocks; block++) {
        const LirBlock *bb = &lf->blocks[block];

        for (int i = 0; i < bb->ninstr; i++) {
            const Instr *ins = &bb->instrs[i];

            if (ins->position != position)
                continue;
            int src = coalesce_source(ins);
            if ((ins->dst == a && src == b) ||
                (ins->dst == b && src == a))
                return 1;
        }
    }
    return 0;
}

void regalloc_verify(const LirFn *lf, const Liveness *lv,
                     const TargetDesc *td, const AllocResult *alloc)
{
    int registers = 0;
    int spills = 0;

    for (int v = 0; v < lf->nvreg; v++) {
        const LiveInterval *iv = &lv->by_vreg[v];
        int phys;

        if (iv->end < 0)
            continue;
        phys = alloc->vreg_reg[v];
        if (phys < 0) {
            if (alloc->vreg_off[v] == 0)
                diag_fatal("register allocation left live v%d unassigned", v);
            if ((-alloc->vreg_off[v]) %
                lir_type_storage_align(lir_vreg_type(lf, v)) != 0)
                diag_fatal("register allocation misaligned spill for v%d", v);
            spills++;
            continue;
        }
        if (lir_vreg_class(lf, v) == REG_CLASS_MEMORY)
            diag_fatal("register allocation assigned memory-only v%d to a register", v);
        registers++;
        {
            RegClass expected = lir_vreg_class(lf, v);
            RegClass actual = phys >= PHYS_XMM0 ? REG_CLASS_XMM : REG_CLASS_GPR;
            if (expected != actual)
                diag_fatal("register allocation assigned v%d to wrong register class", v);
        }
        if (lir_vreg_precolor(lf, v) >= 0) {
            if (phys != lir_vreg_precolor(lf, v))
                diag_fatal("register allocation changed precolored v%d", v);
        } else {
            if (reg_blocked(lv, phys, iv))
                diag_fatal("register allocation overlaps fixed register for v%d", v);
            if (interval_spans_call(lf, iv) &&
                (td->caller_saved_mask & (1u << phys)))
                diag_fatal("register allocation keeps v%d in caller-saved register across call", v);
        }
    }
    if (registers != alloc->register_vregs || spills != alloc->spilled_vregs)
        diag_fatal("register allocation metrics are inconsistent");

    for (int v = 0; v < lf->nvreg; v++) {
        int phys = alloc->vreg_reg[v];

        if (phys < 0 || lir_vreg_precolor(lf, v) >= 0)
            continue;
        for (int u = 0; u < v; u++) {
            int position;

            if (alloc->vreg_reg[u] != phys ||
                lir_vreg_precolor(lf, u) >= 0 ||
                !intervals_overlap(&lv->by_vreg[u], &lv->by_vreg[v]))
                continue;
            if (!single_overlap_position(&lv->by_vreg[u], &lv->by_vreg[v],
                                         &position) ||
                !coalescing_instruction_at(lf, u, v, position))
                diag_fatal("register allocation overlaps v%d and v%d", u, v);
        }
    }
}

void regalloc_dump(const LirFn *lf, const AllocResult *alloc,
                   const TargetDesc *td, FILE *out)
{
    fprintf(out, "allocation %s\n", lf->name);
    fprintf(out,
            "  metrics live=%d registers=%d spilled=%d spill-slots=%d "
            "callee-saved=%d range-reuses=%d splits=%d fragments=%d "
            "split-moves=%d outgoing=%d frame=%d\n",
            alloc->live_vregs, alloc->register_vregs, alloc->spilled_vregs,
            alloc->spill_slots, mask_count(alloc->used_callee_saved),
            alloc->range_reuses, alloc->split_vregs, alloc->split_fragments,
            alloc->split_moves, alloc->outgoing_size, alloc->frame_size);
    for (int v = 0; v < lf->nvreg; v++) {
        int parent = alloc->fragment_parent
            ? alloc->fragment_parent[v] : LIR_NO_VREG;

        if (alloc->vreg_reg[v] >= 0) {
            fprintf(out, "  v%d", v);
            if (parent >= 0)
                fprintf(out, " fragment-of=v%d", parent);
            fprintf(out, " = %s\n", td->reg_name(alloc->vreg_reg[v], 8));
        } else if (alloc->vreg_off[v] != 0) {
            fprintf(out, "  v%d", v);
            if (parent >= 0)
                fprintf(out, " fragment-of=v%d", parent);
            fprintf(out, " = stack(%d)\n", alloc->vreg_off[v]);
        }
    }
}
