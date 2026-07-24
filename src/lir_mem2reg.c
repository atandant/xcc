/* SPDX-License-Identifier: MIT */
#include "lir_mem2reg.h"

#include "arena.h"
#include "diag.h"
#include "lir_dom.h"

#include <string.h>

typedef struct {
    FrameLocal *frame;
    int eligible;
    int has_load;
    Instr load_template;
    int seed;
} PromoteSlot;

typedef struct {
    LirFn *fn;
    const LirDom *dom;
    PromoteSlot *slots;
    int nslots;
    int *phi_vreg;
    int *current;
    int *alias;
} RenameCtx;

static int direct_frame_operand(Operand operand, long *offset)
{
    if (operand.kind != OPND_MEM || operand.u.mem.base != LIR_FP ||
        operand.u.mem.index != LIR_NO_IDX)
        return 0;
    *offset = operand.u.mem.disp;
    return 1;
}

static int slot_at_offset(PromoteSlot *slots, int nslots, long offset)
{
    for (int s = 0; s < nslots; s++) {
        if (slots[s].frame->offset == offset)
            return s;
    }
    return -1;
}

static int operand_overlaps_slot(Operand operand, const PromoteSlot *slot)
{
    long offset;
    long start = slot->frame->offset;
    long end = start + slot->frame->size;

    return direct_frame_operand(operand, &offset) &&
           offset >= start && offset < end;
}

static void disqualify_operand_uses(PromoteSlot *slots, int nslots,
                                    Operand operand)
{
    for (int s = 0; s < nslots; s++) {
        if (slots[s].eligible && operand_overlaps_slot(operand, &slots[s]))
            slots[s].eligible = 0;
    }
}

static int same_load_shape(const Instr *a, const Instr *b)
{
    return a->w == b->w && a->sgn == b->sgn && a->aux == b->aux;
}

static void find_promotable_slots(LirFn *fn, const LirDom *dom,
                                  PromoteSlot *slots)
{
    int nslots = fn->nframe_locals;

    for (int s = 0; s < nslots; s++) {
        slots[s].frame = &fn->frame_locals[s];
        /* Do not promote char or short yet.  Their stores truncate and their
           loads sign- or zero-extend, so direct vreg substitution would change
           values.  Narrow promotion must normalize every definition. */
        slots[s].eligible = slots[s].frame->promotable_scalar &&
                            !slots[s].frame->address_taken;
        slots[s].seed = LIR_NO_VREG;
    }

    for (int b = 0; b < fn->nblocks; b++) {
        LirBlock *block = &fn->blocks[b];

        for (int i = 0; i < block->ninstr; i++) {
            Instr *ins = &block->instrs[i];
            long offset;
            int slot = direct_frame_operand(ins->a, &offset)
                       ? slot_at_offset(slots, nslots, offset) : -1;

            if ((ins->op == LIR_LOAD || ins->op == LIR_STORE) && slot >= 0 &&
                slots[slot].eligible) {
                if (!dom->reachable[b] || ins->aux != slots[slot].frame->size ||
                    (ins->op == LIR_STORE && ins->b.kind != OPND_VREG)) {
                    slots[slot].eligible = 0;
                    continue;
                }
                if (ins->op == LIR_LOAD) {
                    if (slots[slot].has_load &&
                        !same_load_shape(&slots[slot].load_template, ins)) {
                        slots[slot].eligible = 0;
                    } else if (!slots[slot].has_load) {
                        slots[slot].has_load = 1;
                        slots[slot].load_template = *ins;
                    }
                }
                continue;
            }

            disqualify_operand_uses(slots, nslots, ins->a);
            disqualify_operand_uses(slots, nslots, ins->b);
            if (ins->op == LIR_CALL) {
                for (int a = 0; a < ins->nargs; a++)
                    disqualify_operand_uses(slots, nslots, ins->call_args[a]);
            }
        }

        disqualify_operand_uses(slots, nslots, block->term.a);
        disqualify_operand_uses(slots, nslots, block->term.b);
    }

    for (int s = 0; s < nslots; s++) {
        if (!slots[s].has_load)
            slots[s].eligible = 0;
    }
}

static int count_eligible(const PromoteSlot *slots, int nslots)
{
    int count = 0;
    for (int s = 0; s < nslots; s++)
        count += slots[s].eligible != 0;
    return count;
}

static int count_seed_loads(const LirFn *fn, const PromoteSlot *slots,
                            int nslots)
{
    int count = 0;

    for (int s = 0; s < nslots; s++) {
        if (slots[s].eligible &&
            lir_home_vreg(fn, slots[s].frame->offset) == LIR_NO_VREG)
            count++;
    }
    return count;
}

static void insert_seed_loads(LirFn *fn, PromoteSlot *slots, int nslots)
{
    LirBlock *entry = &fn->blocks[fn->entry_block];
    int nseed = count_seed_loads(fn, slots, nslots);
    Instr *instrs = arena_alloc((size_t)(entry->ninstr + nseed) * sizeof(*instrs));
    int out = 0;

    /* Seed every promoted object so paths that read before storing always have
       an SSA value.  Register parameters already have an incoming home vreg;
       DCE removes memory seeds made dead by stores on every path. */
    for (int s = 0; s < nslots; s++) {
        Instr seed;
        int home;

        if (!slots[s].eligible)
            continue;
        home = lir_home_vreg(fn, slots[s].frame->offset);
        if (home != LIR_NO_VREG) {
            slots[s].seed = home;
            continue;
        }
        seed = slots[s].load_template;
        seed.dst = lir_new_vreg(fn);
        seed.position = 0;
        slots[s].seed = seed.dst;
        instrs[out++] = seed;
    }
    memcpy(&instrs[out], entry->instrs,
           (size_t)entry->ninstr * sizeof(*entry->instrs));
    entry->instrs = instrs;
    entry->ninstr += nseed;
    entry->cap = entry->ninstr;
}

static int instruction_slot(const Instr *ins, PromoteSlot *slots, int nslots)
{
    long offset;
    int slot;

    if (ins->op != LIR_LOAD && ins->op != LIR_STORE)
        return -1;
    if (!direct_frame_operand(ins->a, &offset))
        return -1;
    slot = slot_at_offset(slots, nslots, offset);
    return slot >= 0 && slots[slot].eligible ? slot : -1;
}

static void place_phis(LirFn *fn, const LirDom *dom, PromoteSlot *slots,
                       int nslots, int *phi_vreg)
{
    int nblocks = fn->nblocks;

    for (int s = 0; s < nslots; s++) {
        unsigned char *defs;
        unsigned char *queued;
        int *work;
        int nwork = 0;

        if (!slots[s].eligible)
            continue;
        defs = arena_alloc_zeroed((size_t)nblocks);
        queued = arena_alloc_zeroed((size_t)nblocks);
        work = arena_alloc((size_t)nblocks * sizeof(*work));
        for (int b = 0; b < nblocks; b++) {
            LirBlock *block = &fn->blocks[b];

            if (!dom->reachable[b])
                continue;
            for (int i = 0; i < block->ninstr; i++) {
                if (block->instrs[i].op == LIR_STORE &&
                    instruction_slot(&block->instrs[i], slots, nslots) == s) {
                    defs[b] = 1;
                    break;
                }
            }
            if (defs[b]) {
                queued[b] = 1;
                work[nwork++] = b;
            }
        }

        for (int at = 0; at < nwork; at++) {
            int block = work[at];

            for (int member = 0; member < nblocks; member++) {
                int index = s * nblocks + member;

                if (!lir_dom_in_frontier(dom, block, member) ||
                    phi_vreg[index] != LIR_NO_VREG)
                    continue;
                phi_vreg[index] = lir_new_vreg(fn);
                lir_block_add_phi(&fn->blocks[member], phi_vreg[index]);
                if (!queued[member]) {
                    queued[member] = 1;
                    work[nwork++] = member;
                }
            }
        }
    }
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
    if (operand->kind == OPND_VREG) {
        operand->u.vreg = resolve_alias(alias, operand->u.vreg);
    } else if (operand->kind == OPND_MEM) {
        if (operand->u.mem.base != LIR_FP)
            operand->u.mem.base = resolve_alias(alias, operand->u.mem.base);
        if (operand->u.mem.index != LIR_NO_IDX)
            operand->u.mem.index = resolve_alias(alias, operand->u.mem.index);
    }
}

static void rewrite_instruction_operands(Instr *ins, int *alias)
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

static void add_successor_phi_inputs(RenameCtx *ctx, int pred, int successor)
{
    int nblocks = ctx->fn->nblocks;

    for (int s = 0; s < ctx->nslots; s++) {
        int phi = ctx->phi_vreg[s * nblocks + successor];

        if (ctx->slots[s].eligible && phi != LIR_NO_VREG) {
            LirBlock *block = &ctx->fn->blocks[successor];
            for (int p = 0; p < block->nphis; p++) {
                if (block->phis[p].dst == phi) {
                    lir_phi_add_input(&block->phis[p], pred,
                                      resolve_alias(ctx->alias, ctx->current[s]));
                    break;
                }
            }
        }
    }
}

static void rename_block(RenameCtx *ctx, int block_id)
{
    LirBlock *block = &ctx->fn->blocks[block_id];
    int *saved = arena_alloc((size_t)ctx->nslots * sizeof(*saved));
    int out = 0;

    memcpy(saved, ctx->current, (size_t)ctx->nslots * sizeof(*saved));
    for (int s = 0; s < ctx->nslots; s++) {
        int phi = ctx->phi_vreg[s * ctx->fn->nblocks + block_id];
        if (ctx->slots[s].eligible && phi != LIR_NO_VREG)
            ctx->current[s] = phi;
    }

    for (int i = 0; i < block->ninstr; i++) {
        Instr ins = block->instrs[i];
        int slot;

        rewrite_instruction_operands(&ins, ctx->alias);
        slot = instruction_slot(&ins, ctx->slots, ctx->nslots);
        if (slot >= 0 && ins.op == LIR_LOAD &&
            ins.dst != ctx->slots[slot].seed) {
            ctx->alias[ins.dst] = resolve_alias(ctx->alias, ctx->current[slot]);
            continue;
        }
        if (slot >= 0 && ins.op == LIR_STORE) {
            ctx->current[slot] = ins.b.u.vreg;
            continue;
        }
        block->instrs[out++] = ins;
    }
    block->ninstr = out;

    rewrite_operand(&block->term.a, ctx->alias);
    rewrite_operand(&block->term.b, ctx->alias);
    if (block->term.kind == LIR_TERM_JMP) {
        add_successor_phi_inputs(ctx, block_id, block->term.target);
    } else if (block->term.kind == LIR_TERM_BR) {
        add_successor_phi_inputs(ctx, block_id, block->term.true_target);
        if (block->term.false_target != block->term.true_target)
            add_successor_phi_inputs(ctx, block_id, block->term.false_target);
    }

    for (int child = 0; child < ctx->fn->nblocks; child++) {
        if (lir_dom_is_child(ctx->dom, block_id, child))
            rename_block(ctx, child);
    }
    memcpy(ctx->current, saved, (size_t)ctx->nslots * sizeof(*saved));
}

static int phi_has_pred(const LirPhi *phi, int pred)
{
    for (int i = 0; i < phi->ninputs; i++) {
        if (phi->inputs[i].pred == pred)
            return 1;
    }
    return 0;
}

static void finish_phi_inputs(RenameCtx *ctx)
{
    int nblocks = ctx->fn->nblocks;

    for (int b = 0; b < nblocks; b++) {
        LirBlock *block = &ctx->fn->blocks[b];

        for (int p = 0; p < block->nphis; p++) {
            LirPhi *phi = &block->phis[p];

            for (int i = 0; i < phi->ninputs; i++)
                phi->inputs[i].value = resolve_alias(ctx->alias,
                                                     phi->inputs[i].value);
        }
        for (int s = 0; s < ctx->nslots; s++) {
            int phi_vreg = ctx->phi_vreg[s * nblocks + b];

            if (!ctx->slots[s].eligible || phi_vreg == LIR_NO_VREG)
                continue;
            for (int p = 0; p < block->nphis; p++) {
                LirPhi *phi = &block->phis[p];

                if (phi->dst != phi_vreg)
                    continue;
                for (int pred = 0; pred < block->npreds; pred++) {
                    int pred_id = block->preds[pred];
                    if (!phi_has_pred(phi, pred_id))
                        lir_phi_add_input(phi, pred_id, ctx->slots[s].seed);
                }
            }
        }
    }
}

int lir_promote_memory_to_registers(LirFn *fn)
{
    LirDom dom;
    PromoteSlot *slots;
    int *phi_vreg;
    RenameCtx rename;
    int nslots = fn->nframe_locals;

    if (fn->stage != LIR_STAGE_SSA)
        diag_fatal("cannot promote memory in non-SSA LIR function '%s'", fn->name);
    if (nslots == 0)
        return 0;

    lir_dom_compute(fn, &dom);
    slots = arena_alloc_zeroed((size_t)nslots * sizeof(*slots));
    find_promotable_slots(fn, &dom, slots);
    if (count_eligible(slots, nslots) == 0)
        return 0;

    insert_seed_loads(fn, slots, nslots);
    phi_vreg = arena_alloc((size_t)nslots * (size_t)fn->nblocks *
                           sizeof(*phi_vreg));
    for (int i = 0; i < nslots * fn->nblocks; i++)
        phi_vreg[i] = LIR_NO_VREG;
    place_phis(fn, &dom, slots, nslots, phi_vreg);

    rename = (RenameCtx){
        .fn = fn,
        .dom = &dom,
        .slots = slots,
        .nslots = nslots,
        .phi_vreg = phi_vreg,
        .current = arena_alloc((size_t)nslots * sizeof(*rename.current)),
        .alias = arena_alloc((size_t)fn->nvreg * sizeof(*rename.alias)),
    };
    for (int s = 0; s < nslots; s++)
        rename.current[s] = slots[s].seed;
    for (int v = 0; v < fn->nvreg; v++)
        rename.alias[v] = v;
    rename_block(&rename, fn->entry_block);
    finish_phi_inputs(&rename);
    return 1;
}
