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
            }
            if (lir_instruction_defines_vreg(ins)) {
                if (ins->dst >= fn->nvreg)
                    malformed(fn, i, "instruction defines an invalid vreg");
                if (fn->stage == LIR_STAGE_SSA && defined[ins->dst])
                    malformed(fn, i, "vreg has multiple SSA definitions");
                defined[ins->dst] = 1;
            }
        }
        if (block->term.kind == LIR_TERM_BR) {
            verify_operand(fn, i, block->term.a);
            verify_operand(fn, i, block->term.b);
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
            tmp = lir_new_vreg(fn);
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

void lir_cfg_number_instructions(LirFn *fn)
{
    int position = 0;

    for (int b = 0; b < fn->nblocks; b++) {
        LirBlock *block;
        if (b == fn->epilogue_label)
            continue;
        block = &fn->blocks[b];
        block->start_position = position;
        for (int i = 0; i < block->ninstr; i++)
            block->instrs[i].position = position++;
        block->term.position = position++;
        block->end_position = block->term.position;
    }
    LirBlock *epilogue = &fn->blocks[fn->epilogue_label];
    epilogue->start_position = position;
    for (int i = 0; i < epilogue->ninstr; i++)
        epilogue->instrs[i].position = position++;
    epilogue->term.position = position++;
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
    lir_cfg_number_instructions(fn);
}
