/* SPDX-License-Identifier: MIT */
#include "lir_cfg.h"

#include "arena.h"
#include "diag.h"

#include <string.h>

static int valid_block(const LirFn *fn, LirBlockId id)
{
    return id >= 0 && id < fn->nblocks;
}

static void add_pred(LirBlock *block, LirBlockId pred)
{
    for (int i = 0; i < block->npreds; i++) {
        if (block->preds[i] == pred)
            return;
    }
    if (block->npreds >= block->preds_cap) {
        int new_cap = block->preds_cap ? block->preds_cap * 2 : 4;
        LirBlockId *n = arena_alloc((size_t)new_cap * sizeof(*n));
        if (block->preds)
            memcpy(n, block->preds,
                   (size_t)block->npreds * sizeof(*n));
        block->preds = n;
        block->preds_cap = new_cap;
    }
    block->preds[block->npreds++] = pred;
}

void lir_cfg_rebuild_preds(LirFn *fn)
{
    for (int i = 0; i < fn->nblocks; i++)
        fn->blocks[i].npreds = 0;

    for (int i = 0; i < fn->nblocks; i++) {
        LirTerminator *term = &fn->blocks[i].term;

        if (term->kind == LIR_TERM_JMP) {
            if (valid_block(fn, term->target))
                add_pred(&fn->blocks[term->target], i);
        } else if (term->kind == LIR_TERM_BR) {
            if (valid_block(fn, term->true_target))
                add_pred(&fn->blocks[term->true_target], i);
            if (valid_block(fn, term->false_target))
                add_pred(&fn->blocks[term->false_target], i);
        }
    }
}

static void malformed(const LirFn *fn, int block, const char *what)
{
    if (block >= 0)
        diag_fatal("malformed LIR in function '%s', bb%d: %s",
                   fn->name, block, what);
    diag_fatal("malformed LIR in function '%s': %s", fn->name, what);
}

static int has_pred(const LirBlock *block, LirBlockId pred)
{
    for (int i = 0; i < block->npreds; i++) {
        if (block->preds[i] == pred)
            return 1;
    }
    return 0;
}

static void verify_operand(const LirFn *fn, int block, Operand operand)
{
    if (operand.kind == OPND_VREG &&
        (operand.u.vreg < 0 || operand.u.vreg >= fn->nvreg))
        malformed(fn, block, "instruction uses an invalid vreg");
    if (operand.kind == OPND_MEM) {
        if (operand.u.mem.base != LIR_FP &&
            (operand.u.mem.base < 0 || operand.u.mem.base >= fn->nvreg))
            malformed(fn, block, "memory operand has an invalid base vreg");
        if (operand.u.mem.index != LIR_NO_IDX &&
            (operand.u.mem.index < 0 || operand.u.mem.index >= fn->nvreg))
            malformed(fn, block, "memory operand has an invalid index vreg");
    }
}

static int operand_is_type(const LirFn *fn, Operand operand, LirType type)
{
    return operand.kind == OPND_VREG &&
           lir_vreg_type(fn, operand.u.vreg) == type;
}

static void verify_f80_instr(const LirFn *fn, int block, const Instr *ins)
{
    if (ins->op == LIR_LOAD && ins->dst >= 0 &&
        lir_vreg_type(fn, ins->dst) == LIR_TYPE_F80) {
        if (ins->aux != 16 || ins->fpw != LIR_FP_F80)
            malformed(fn, block, "F80 load has an invalid width");
        return;
    }

    if (ins->op == LIR_STORE && ins->b.kind == OPND_VREG &&
        lir_vreg_type(fn, ins->b.u.vreg) == LIR_TYPE_F80) {
        if (ins->aux != 16 || ins->fpw != LIR_FP_F80)
            malformed(fn, block, "F80 store has an invalid width");
        return;
    }

    if (ins->op == LIR_MOV && ins->dst >= 0 &&
        lir_vreg_type(fn, ins->dst) == LIR_TYPE_F80) {
        if (!operand_is_type(fn, ins->a, LIR_TYPE_F80))
            malformed(fn, block, "F80 move has a non-F80 source");
        return;
    }

    if ((ins->op == LIR_FADD || ins->op == LIR_FSUB ||
         ins->op == LIR_FMUL || ins->op == LIR_FDIV) &&
        ins->fpw == LIR_FP_F80) {
        if (ins->dst < 0 || lir_vreg_type(fn, ins->dst) != LIR_TYPE_F80 ||
            !operand_is_type(fn, ins->a, LIR_TYPE_F80) ||
            !operand_is_type(fn, ins->b, LIR_TYPE_F80))
            malformed(fn, block, "F80 arithmetic mixes value types");
        return;
    }

    if (ins->op == LIR_FNEG && ins->fpw == LIR_FP_F80) {
        if (ins->dst < 0 || lir_vreg_type(fn, ins->dst) != LIR_TYPE_F80 ||
            !operand_is_type(fn, ins->a, LIR_TYPE_F80))
            malformed(fn, block, "F80 negation mixes value types");
        return;
    }

    if (ins->op == LIR_FSETCC && ins->fpw == LIR_FP_F80) {
        if (ins->dst < 0 || lir_vreg_type(fn, ins->dst) != LIR_TYPE_I64 ||
            !operand_is_type(fn, ins->a, LIR_TYPE_F80) ||
            !operand_is_type(fn, ins->b, LIR_TYPE_F80))
            malformed(fn, block, "F80 comparison mixes value types");
        return;
    }

    if (ins->op == LIR_FRET) {
        if (ins->fpw != LIR_FP_F80 ||
            !operand_is_type(fn, ins->a, LIR_TYPE_F80))
            malformed(fn, block, "F80 return has an invalid value");
        return;
    }

    if (ins->op != LIR_CONV)
        return;
    if (ins->conv == CONV_F32_F80 || ins->conv == CONV_F64_F80) {
        LirType source = ins->conv == CONV_F32_F80
            ? LIR_TYPE_F32 : LIR_TYPE_F64;
        if (ins->dst < 0 || lir_vreg_type(fn, ins->dst) != LIR_TYPE_F80 ||
            !operand_is_type(fn, ins->a, source))
            malformed(fn, block, "conversion to F80 mixes value types");
    } else if (ins->conv == CONV_F80_F32 || ins->conv == CONV_F80_F64) {
        LirType destination = ins->conv == CONV_F80_F32
            ? LIR_TYPE_F32 : LIR_TYPE_F64;
        if (ins->dst < 0 || lir_vreg_type(fn, ins->dst) != destination ||
            !operand_is_type(fn, ins->a, LIR_TYPE_F80))
            malformed(fn, block, "conversion from F80 mixes value types");
    }
}

void lir_cfg_verify(const LirFn *fn)
{
    unsigned char *defined;

    if (!fn)
        diag_fatal("malformed LIR: null function");
    if (!valid_block(fn, fn->entry_block))
        malformed(fn, -1, "invalid entry block");
    defined = arena_alloc_zeroed((size_t)fn->nvreg);

    for (int i = 0; i < fn->nblocks; i++) {
        const LirBlock *block = &fn->blocks[i];

        if (block->id != i)
            malformed(fn, i, "block ID does not match its table index");
        if (block->term.kind == LIR_TERM_NONE)
            malformed(fn, i, "block has no terminator");
        if (block->term.kind == LIR_TERM_JMP &&
            !valid_block(fn, block->term.target))
            malformed(fn, i, "jump has an invalid target");
        if (block->term.kind == LIR_TERM_BR &&
            (!valid_block(fn, block->term.true_target) ||
             !valid_block(fn, block->term.false_target)))
            malformed(fn, i, "branch has an invalid target");
        if (fn->stage == LIR_STAGE_LOWERED && block->nphis != 0)
            malformed(fn, i, "lowered LIR still contains phi nodes");

        for (int j = 0; j < block->ninstr; j++) {
            const Instr *ins = &block->instrs[j];

            if (ins->op == LIR_LABEL || ins->op == LIR_JMP ||
                ins->op == LIR_BR || ins->op == LIR_RET)
                malformed(fn, i, "control instruction appears in a block body");
            verify_operand(fn, i, ins->a);
            verify_operand(fn, i, ins->b);
            if (ins->op == LIR_CALL) {
                if (ins->call_indirect &&
                    (ins->call_reg < 0 || ins->call_reg >= fn->nvreg))
                    malformed(fn, i, "indirect call uses an invalid vreg");
                for (int a = 0; a < ins->nargs; a++)
                    verify_operand(fn, i, ins->call_args[a]);
                if (ins->call_ret_type == LIR_TYPE_F80) {
                    if (ins->dst < 0 || ins->dst >= fn->nvreg ||
                        lir_vreg_type(fn, ins->dst) != LIR_TYPE_F80)
                        malformed(fn, i, "F80 call has an invalid destination");
                }
            }
            if (lir_instruction_defines_vreg(ins)) {
                if (ins->dst >= fn->nvreg)
                    malformed(fn, i, "instruction defines an invalid vreg");
                if (fn->stage == LIR_STAGE_SSA && defined[ins->dst])
                    malformed(fn, i, "vreg has multiple SSA definitions");
                defined[ins->dst] = 1;
            }
            verify_f80_instr(fn, i, ins);
        }
        if (block->term.kind == LIR_TERM_BR) {
            verify_operand(fn, i, block->term.a);
            verify_operand(fn, i, block->term.b);
            if (block->term.fpw == LIR_FP_F80 &&
                (!operand_is_type(fn, block->term.a, LIR_TYPE_F80) ||
                 !operand_is_type(fn, block->term.b, LIR_TYPE_F80)))
                malformed(fn, i, "F80 branch mixes value types");
        } else if (block->term.kind == LIR_TERM_RET) {
            verify_operand(fn, i, block->term.a);
        }

        for (int p = 0; p < block->npreds; p++) {
            if (!valid_block(fn, block->preds[p]))
                malformed(fn, i, "predecessor list contains an invalid block");
            for (int q = p + 1; q < block->npreds; q++) {
                if (block->preds[p] == block->preds[q])
                    malformed(fn, i, "predecessor list contains a duplicate");
            }
        }

        for (int p = 0; p < block->nphis; p++) {
            const LirPhi *phi = &block->phis[p];

            if (phi->dst < 0 || phi->dst >= fn->nvreg)
                malformed(fn, i, "phi has an invalid destination");
            if (fn->stage == LIR_STAGE_SSA && defined[phi->dst])
                malformed(fn, i, "vreg has multiple SSA definitions");
            defined[phi->dst] = 1;
            if (phi->ninputs != block->npreds)
                malformed(fn, i, "phi input count does not match predecessors");
            for (int a = 0; a < phi->ninputs; a++) {
                if (!has_pred(block, phi->inputs[a].pred))
                    malformed(fn, i, "phi input is not a predecessor");
                if (phi->inputs[a].value < 0 ||
                    phi->inputs[a].value >= fn->nvreg)
                    malformed(fn, i, "phi input has an invalid value");
                if (lir_vreg_class(fn, phi->dst) !=
                    lir_vreg_class(fn, phi->inputs[a].value))
                    malformed(fn, i, "phi mixes register classes");
                if (lir_vreg_type(fn, phi->dst) !=
                    lir_vreg_type(fn, phi->inputs[a].value))
                    malformed(fn, i, "phi mixes value types");
                for (int b = a + 1; b < phi->ninputs; b++) {
                    if (phi->inputs[a].pred == phi->inputs[b].pred)
                        malformed(fn, i, "phi has duplicate predecessor inputs");
                }
            }
        }
    }
}

static void rewrite_edge(LirBlock *from, LirBlockId old_target,
                         LirBlockId new_target)
{
    if (from->term.kind == LIR_TERM_JMP) {
        if (from->term.target == old_target)
            from->term.target = new_target;
        return;
    }
    if (from->term.kind == LIR_TERM_BR) {
        if (from->term.true_target == old_target)
            from->term.true_target = new_target;
        if (from->term.false_target == old_target)
            from->term.false_target = new_target;
    }
}

static LirBlockId split_edge(LirFn *fn, LirBlockId from, LirBlockId to)
{
    LirBlockId edge = lir_new_block(fn);
    LirBlock *edge_block = lir_get_block(fn, edge);

    rewrite_edge(lir_get_block(fn, from), to, edge);
    edge_block->term.kind = LIR_TERM_JMP;
    edge_block->term.target = to;
    for (int p = 0; p < fn->blocks[to].nphis; p++) {
        LirPhi *phi = &fn->blocks[to].phis[p];
        for (int a = 0; a < phi->ninputs; a++) {
            if (phi->inputs[a].pred == from)
                phi->inputs[a].pred = edge;
        }
    }
    return edge;
}

static int block_has_two_successors(const LirBlock *block)
{
    return block->term.kind == LIR_TERM_BR &&
           block->term.true_target != block->term.false_target;
}

static void append_copy(LirBlock *block, int dst, int src)
{
    if (dst == src)
        return;
    lir_block_emit(block, (Instr){
        .op = LIR_MOV,
        .dst = dst,
        .a = lir_vreg(src),
    });
}

static void lower_phi_copies(LirFn *fn, LirBlockId block_id,
                             LirBlockId pred)
{
    LirBlock *block = lir_get_block(fn, block_id);
    int n = block->nphis;
    int *dst = arena_alloc((size_t)n * sizeof(*dst));
    int *src = arena_alloc((size_t)n * sizeof(*src));
    unsigned char *done = arena_alloc_zeroed((size_t)n);
    LirBlock *insert = lir_get_block(fn, pred);
    int remaining = 0;

    for (int p = 0; p < n; p++) {
        dst[p] = block->phis[p].dst;
        src[p] = -1;
        for (int a = 0; a < block->phis[p].ninputs; a++) {
            if (block->phis[p].inputs[a].pred == pred) {
                src[p] = block->phis[p].inputs[a].value;
                break;
            }
        }
        if (src[p] < 0)
            malformed(fn, block_id, "phi input disappeared during lowering");
        if (dst[p] == src[p])
            done[p] = 1;
        else
            remaining++;
    }

    while (remaining) {
        int progress = 0;

        for (int i = 0; i < n; i++) {
            int dst_is_source = 0;
            if (done[i])
                continue;
            for (int j = 0; j < n; j++) {
                if (!done[j] && src[j] == dst[i]) {
                    dst_is_source = 1;
                    break;
                }
            }
            if (dst_is_source)
                continue;
            append_copy(insert, dst[i], src[i]);
            done[i] = 1;
            remaining--;
            progress = 1;
        }

        if (!progress) {
            int first = 0;
            int tmp;
            while (done[first])
                first++;
            tmp = lir_new_vreg_type(fn, lir_vreg_type(fn, src[first]));
            append_copy(insert, tmp, src[first]);
            for (int i = 0; i < n; i++) {
                if (!done[i] && src[i] == src[first])
                    src[i] = tmp;
            }
        }
    }
}

static void eliminate_phis(LirFn *fn)
{
    int original_blocks = fn->nblocks;

    lir_cfg_rebuild_preds(fn);
    for (int b = 0; b < original_blocks; b++) {
        LirBlock *block = &fn->blocks[b];
        if (block->nphis == 0)
            continue;

        int npreds = block->npreds;
        LirBlockId *preds = arena_alloc((size_t)npreds * sizeof(*preds));
        memcpy(preds, block->preds, (size_t)npreds * sizeof(*preds));
        for (int p = 0; p < npreds; p++) {
            LirBlockId pred = preds[p];
            if (block_has_two_successors(lir_get_block(fn, pred)))
                pred = split_edge(fn, pred, b);
            lower_phi_copies(fn, b, pred);
        }
        block = &fn->blocks[b];
        block->nphis = 0;
        lir_cfg_rebuild_preds(fn);
    }
}

static int forwarding_target(const LirFn *fn, int block)
{
    int target = block;

    for (int steps = 0; steps < fn->nblocks; steps++) {
        const LirBlock *next = &fn->blocks[target];

        if (target == fn->epilogue_label || next->ninstr != 0 ||
            next->term.kind != LIR_TERM_JMP || next->term.target == target)
            break;
        target = next->term.target;
    }
    return target;
}

static int thread_forwarding_blocks(LirFn *fn)
{
    int changed = 0;

    for (int b = 0; b < fn->nblocks; b++) {
        LirTerminator *term = &fn->blocks[b].term;

        if (term->kind == LIR_TERM_JMP) {
            int target = forwarding_target(fn, term->target);
            if (target != term->target) {
                term->target = target;
                changed = 1;
            }
        } else if (term->kind == LIR_TERM_BR) {
            int true_target = forwarding_target(fn, term->true_target);
            int false_target = forwarding_target(fn, term->false_target);

            if (true_target != term->true_target ||
                false_target != term->false_target) {
                term->true_target = true_target;
                term->false_target = false_target;
                changed = 1;
            }
            if (term->true_target == term->false_target) {
                term->kind = LIR_TERM_JMP;
                term->target = term->true_target;
                changed = 1;
            }
        }
    }
    return changed;
}

static int merge_single_predecessor_blocks(LirFn *fn)
{
    int changed = 0;

    lir_cfg_rebuild_preds(fn);
    for (int b = 0; b < fn->nblocks; b++) {
        LirBlock *block = &fn->blocks[b];
        int successor;

        if (block->term.kind != LIR_TERM_JMP)
            continue;
        successor = block->term.target;
        if (successor == b || successor == fn->entry_block ||
            successor == fn->epilogue_label ||
            fn->blocks[successor].npreds != 1)
            continue;

        LirBlock *next = &fn->blocks[successor];
        for (int i = 0; i < next->ninstr; i++)
            lir_block_emit(block, next->instrs[i]);
        block->term = next->term;
        next->ninstr = 0;
        next->term.kind = LIR_TERM_JMP;
        next->term.target = successor;
        changed = 1;
        lir_cfg_rebuild_preds(fn);
    }
    return changed;
}

static void mark_cfg_reachable(const LirFn *fn, int block,
                               unsigned char *reachable)
{
    const LirTerminator *term;

    if (!valid_block(fn, block) || reachable[block])
        return;
    reachable[block] = 1;
    term = &fn->blocks[block].term;
    if (term->kind == LIR_TERM_JMP) {
        mark_cfg_reachable(fn, term->target, reachable);
    } else if (term->kind == LIR_TERM_BR) {
        mark_cfg_reachable(fn, term->true_target, reachable);
        mark_cfg_reachable(fn, term->false_target, reachable);
    }
}

static int remove_unreachable_blocks(LirFn *fn)
{
    int old_nblocks = fn->nblocks;
    unsigned char *reachable = arena_alloc_zeroed((size_t)old_nblocks);
    int *map = arena_alloc((size_t)old_nblocks * sizeof(*map));
    int nblocks = 0;

    mark_cfg_reachable(fn, fn->entry_block, reachable);
    reachable[fn->epilogue_label] = 1;
    for (int b = 0; b < old_nblocks; b++) {
        map[b] = reachable[b] ? nblocks++ : LIR_NO_BLOCK;
    }
    if (nblocks == old_nblocks)
        return 0;

    LirBlock *blocks = arena_alloc_zeroed((size_t)fn->blocks_cap *
                                          sizeof(*blocks));
    for (int b = 0; b < old_nblocks; b++) {
        if (!reachable[b])
            continue;
        blocks[map[b]] = fn->blocks[b];
        blocks[map[b]].id = map[b];
    }
    fn->blocks = blocks;
    fn->nblocks = nblocks;
    fn->entry_block = map[fn->entry_block];
    fn->epilogue_label = map[fn->epilogue_label];

    for (int b = 0; b < fn->nblocks; b++) {
        LirTerminator *term = &fn->blocks[b].term;
        if (term->kind == LIR_TERM_JMP) {
            term->target = map[term->target];
        } else if (term->kind == LIR_TERM_BR) {
            term->true_target = map[term->true_target];
            term->false_target = map[term->false_target];
        }
    }
    for (int label = 0; label < fn->label_count; label++) {
        int block = fn->label_blocks[label];
        fn->label_blocks[label] = block >= 0 && block < old_nblocks
                                  ? map[block] : LIR_NO_BLOCK;
    }
    lir_cfg_rebuild_preds(fn);
    return 1;
}

int lir_cfg_simplify(LirFn *fn)
{
    int any_changed = 0;

    if (fn->stage != LIR_STAGE_LOWERED)
        diag_fatal("cannot simplify non-lowered CFG in function '%s'", fn->name);
    for (;;) {
        int changed = 0;

        changed |= thread_forwarding_blocks(fn);
        changed |= merge_single_predecessor_blocks(fn);
        changed |= remove_unreachable_blocks(fn);
        any_changed |= changed;
        if (!changed)
            break;
    }
    lir_cfg_rebuild_preds(fn);
    return any_changed;
}

void lir_cfg_number_instructions(LirFn *fn)
{
    int position = 0;

    for (int b = 0; b < fn->nblocks; b++) {
        LirBlock *block;
        if (b == fn->epilogue_label)
            continue;
        block = &fn->blocks[b];
        block->start_position = position;
        for (int i = 0; i < block->ninstr; i++) {
            block->instrs[i].position = position;
            position += 2;
        }
        block->term.position = position;
        position += 2;
        block->end_position = block->term.position;
    }
    LirBlock *epilogue = &fn->blocks[fn->epilogue_label];
    epilogue->start_position = position;
    for (int i = 0; i < epilogue->ninstr; i++) {
        epilogue->instrs[i].position = position;
        position += 2;
    }
    epilogue->term.position = position;
    position += 2;
    epilogue->end_position = epilogue->term.position;
    fn->npositions = position;
}

void lir_cfg_lower(LirFn *fn)
{
    if (fn->stage != LIR_STAGE_SSA)
        diag_fatal("cannot lower non-SSA LIR in function '%s'", fn->name);
    eliminate_phis(fn);
    lir_cfg_rebuild_preds(fn);
    fn->stage = LIR_STAGE_LOWERED;
    lir_cfg_simplify(fn);
    lir_cfg_number_instructions(fn);
}
