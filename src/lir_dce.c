/* SPDX-License-Identifier: MIT */
#include "lir_dce.h"

#include "arena.h"

typedef enum {
    DEF_NONE,
    DEF_INSTRUCTION,
    DEF_PHI,
} DefKind;

typedef struct {
    DefKind kind;
    const Instr *ins;
    const LirPhi *phi;
} Definition;

typedef struct {
    int nvreg;
    unsigned char *live;
    int *work;
    int nwork;
} MarkCtx;

static void mark_vreg(MarkCtx *ctx, int vreg)
{
    if (vreg < 0 || vreg >= ctx->nvreg || ctx->live[vreg])
        return;
    ctx->live[vreg] = 1;
    ctx->work[ctx->nwork++] = vreg;
}

static void mark_operand(MarkCtx *ctx, Operand operand)
{
    if (operand.kind == OPND_VREG) {
        mark_vreg(ctx, operand.u.vreg);
    } else if (operand.kind == OPND_MEM) {
        if (operand.u.mem.base != LIR_FP)
            mark_vreg(ctx, operand.u.mem.base);
        if (operand.u.mem.index != LIR_NO_IDX)
            mark_vreg(ctx, operand.u.mem.index);
    }
}

static void mark_instruction_uses(MarkCtx *ctx, const Instr *ins)
{
    mark_operand(ctx, ins->a);
    mark_operand(ctx, ins->b);
    if (ins->op == LIR_CALL) {
        if (ins->call_indirect)
            mark_vreg(ctx, ins->call_reg);
        for (int a = 0; a < ins->nargs; a++)
            mark_operand(ctx, ins->call_args[a]);
    }
}

static int instruction_has_effect(const Instr *ins)
{
    switch (ins->op) {
    case LIR_MOV:
        /* A destination-less move writes a physical ABI return register. */
        return ins->dst == LIR_NO_VREG;
    case LIR_STORE:
    case LIR_MEMCPY:
    case LIR_CALL:
        return 1;
    case LIR_LOAD:
        /* Ordinary loads are removable today.  When volatile is represented
           in LIR, volatile loads MUST be classified as effectful here. */
        return 0;
    case LIR_DIV:
    case LIR_MOD:
        /* Division by zero is undefined in C89.  Removing an unused division
           cannot introduce a trap on a previously defined execution. */
        return 0;
    default:
        return 0;
    }
}

static void build_definitions(const LirFn *fn, Definition *defs)
{
    for (int b = 0; b < fn->nblocks; b++) {
        const LirBlock *block = &fn->blocks[b];

        for (int p = 0; p < block->nphis; p++) {
            int dst = block->phis[p].dst;

            if (dst >= 0 && dst < fn->nvreg) {
                defs[dst].kind = DEF_PHI;
                defs[dst].phi = &block->phis[p];
            }
        }
        for (int i = 0; i < block->ninstr; i++) {
            const Instr *ins = &block->instrs[i];

            if (lir_instruction_defines_vreg(ins) && ins->dst < fn->nvreg) {
                defs[ins->dst].kind = DEF_INSTRUCTION;
                defs[ins->dst].ins = ins;
            }
        }
    }
}

static void mark_roots(const LirFn *fn, MarkCtx *ctx)
{
    for (int b = 0; b < fn->nblocks; b++) {
        const LirBlock *block = &fn->blocks[b];

        for (int i = 0; i < block->ninstr; i++) {
            if (instruction_has_effect(&block->instrs[i]))
                mark_instruction_uses(ctx, &block->instrs[i]);
        }
        mark_operand(ctx, block->term.a);
        mark_operand(ctx, block->term.b);
    }
}

static void mark_dependencies(const Definition *defs, MarkCtx *ctx)
{
    while (ctx->nwork > 0) {
        const Definition *def = &defs[ctx->work[--ctx->nwork]];

        if (def->kind == DEF_INSTRUCTION) {
            mark_instruction_uses(ctx, def->ins);
        } else if (def->kind == DEF_PHI) {
            for (int i = 0; i < def->phi->ninputs; i++)
                mark_vreg(ctx, def->phi->inputs[i].value);
        }
    }
}

static int sweep(LirFn *fn, const unsigned char *live)
{
    int changed = 0;

    for (int b = 0; b < fn->nblocks; b++) {
        LirBlock *block = &fn->blocks[b];
        int out = 0;

        for (int i = 0; i < block->ninstr; i++) {
            Instr ins = block->instrs[i];
            int defines = lir_instruction_defines_vreg(&ins);

            if (instruction_has_effect(&ins) || !defines || live[ins.dst]) {
                block->instrs[out++] = ins;
            } else {
                changed = 1;
            }
        }
        block->ninstr = out;

        out = 0;
        for (int p = 0; p < block->nphis; p++) {
            LirPhi phi = block->phis[p];

            if (live[phi.dst]) {
                block->phis[out++] = phi;
            } else {
                changed = 1;
            }
        }
        block->nphis = out;
    }
    return changed;
}

int lir_eliminate_dead_code(LirFn *fn)
{
    size_t nvreg = fn->nvreg > 0 ? (size_t)fn->nvreg : 1;
    Definition *defs;
    MarkCtx mark;

    if (fn->stage != LIR_STAGE_SSA)
        return 0;

    defs = arena_alloc_zeroed(nvreg * sizeof(*defs));
    mark = (MarkCtx){
        .nvreg = fn->nvreg,
        .live = arena_alloc_zeroed(nvreg),
        .work = arena_alloc(nvreg * sizeof(*mark.work)),
    };
    build_definitions(fn, defs);
    mark_roots(fn, &mark);
    mark_dependencies(defs, &mark);
    return sweep(fn, mark.live);
}
