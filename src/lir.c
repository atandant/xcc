/* SPDX-License-Identifier: MIT */
#include "lir.h"
#include "arena.h"

#include <assert.h>
#include <string.h>

Operand lir_vreg(int v)
{
    Operand o = { .kind = OPND_VREG, .u.vreg = v };
    return o;
}

Operand lir_phys(int r)
{
    Operand o = { .kind = OPND_PHYS, .u.phys = r };
    return o;
}

Operand lir_imm(long imm)
{
    Operand o = { .kind = OPND_IMM, .u.imm = imm };
    return o;
}

Operand lir_mem(int base, long disp)
{
    Operand o = { .kind = OPND_MEM };
    o.u.mem.base = base;
    o.u.mem.disp = disp;
    o.u.mem.index = LIR_NO_IDX;
    o.u.mem.scale = 1;
    return o;
}

Operand lir_mem_idx(int base, int index, int scale, long disp)
{
    Operand o = { .kind = OPND_MEM };
    o.u.mem.base = base;
    o.u.mem.disp = disp;
    o.u.mem.index = index;
    o.u.mem.scale = scale;
    return o;
}

Operand lir_none(void)
{
    Operand o = { .kind = OPND_NONE };
    return o;
}

LirFn *lir_fn_new(const char *name)
{
    LirFn *fn = arena_alloc_zeroed(sizeof(*fn));
    fn->name = arena_strdup(name);
    fn->blocks_cap = 16;
    fn->blocks = arena_alloc_zeroed((size_t)fn->blocks_cap * sizeof(*fn->blocks));
    fn->cap = 64;
    fn->instrs = arena_alloc((size_t)fn->cap * sizeof(*fn->instrs));
    fn->homes_cap = 32;
    fn->homes = arena_alloc((size_t)fn->homes_cap * sizeof(*fn->homes));
    fn->entry_block = lir_new_block(fn);
    return fn;
}

LirBlockId lir_new_block(LirFn *fn)
{
    if (fn->nblocks >= fn->blocks_cap) {
        int old_cap = fn->blocks_cap;
        fn->blocks_cap *= 2;
        LirBlock *n = arena_alloc_zeroed((size_t)fn->blocks_cap * sizeof(*n));
        memcpy(n, fn->blocks, (size_t)fn->nblocks * sizeof(*n));
        fn->blocks = n;
        (void)old_cap;
    }
    LirBlockId id = fn->nblocks++;
    LirBlock *block = &fn->blocks[id];
    block->id = id;
    block->cap = 16;
    block->instrs = arena_alloc((size_t)block->cap * sizeof(*block->instrs));
    block->phis_cap = 2;
    block->phis = arena_alloc_zeroed((size_t)block->phis_cap * sizeof(*block->phis));
    block->term.kind = LIR_TERM_NONE;
    return id;
}

LirBlock *lir_get_block(LirFn *fn, LirBlockId id)
{
    assert(id >= 0 && id < fn->nblocks);
    return &fn->blocks[id];
}

int lir_block_emit(LirBlock *block, Instr ins)
{
    if (block->ninstr >= block->cap) {
        block->cap *= 2;
        Instr *n = arena_alloc((size_t)block->cap * sizeof(*n));
        memcpy(n, block->instrs, (size_t)block->ninstr * sizeof(*n));
        block->instrs = n;
    }
    int idx = block->ninstr++;
    block->instrs[idx] = ins;
    return idx;
}

LirPhi *lir_block_add_phi(LirBlock *block, int dst)
{
    if (block->nphis >= block->phis_cap) {
        block->phis_cap *= 2;
        LirPhi *n = arena_alloc_zeroed((size_t)block->phis_cap * sizeof(*n));
        memcpy(n, block->phis, (size_t)block->nphis * sizeof(*n));
        block->phis = n;
    }
    LirPhi *phi = &block->phis[block->nphis++];
    memset(phi, 0, sizeof(*phi));
    phi->dst = dst;
    phi->cap = 2;
    phi->inputs = arena_alloc((size_t)phi->cap * sizeof(*phi->inputs));
    return phi;
}

void lir_phi_add_input(LirPhi *phi, LirBlockId pred, int value)
{
    if (phi->ninputs >= phi->cap) {
        phi->cap *= 2;
        PhiInput *n = arena_alloc((size_t)phi->cap * sizeof(*n));
        memcpy(n, phi->inputs, (size_t)phi->ninputs * sizeof(*n));
        phi->inputs = n;
    }
    phi->inputs[phi->ninputs++] = (PhiInput){ .pred = pred, .value = value };
}

int lir_home_vreg(LirFn *lf, int offset)
{
    for (int i = 0; i < lf->nhomes; i++) {
        if (lf->homes[i].offset == offset)
            return lf->homes[i].vreg;
    }
    return LIR_NO_VREG;
}

int lir_is_home_vreg(const LirFn *lf, int vreg)
{
    for (int i = 0; i < lf->nhomes; i++) {
        if (lf->homes[i].vreg == vreg)
            return 1;
    }
    return 0;
}

void lir_bind_home(LirFn *lf, int offset, int vreg)
{
    for (int i = 0; i < lf->nhomes; i++) {
        if (lf->homes[i].offset == offset) {
            lf->homes[i].vreg = vreg;
            return;
        }
    }
    if (lf->nhomes >= lf->homes_cap) {
        lf->homes_cap *= 2;
        LocalHome *n = arena_alloc((size_t)lf->homes_cap * sizeof(*n));
        memcpy(n, lf->homes, (size_t)lf->nhomes * sizeof(*n));
        lf->homes = n;
    }
    lf->homes[lf->nhomes].offset = offset;
    lf->homes[lf->nhomes].vreg = vreg;
    lf->nhomes++;
}

int lir_max_outgoing(const LirFn *lf)
{
    int max_out = 0;

    for (int b = 0; b < lf->nblocks; b++) {
      for (int i = 0; i < lf->blocks[b].ninstr; i++) {
        const Instr *ins = &lf->blocks[b].instrs[i];

        if (ins->op != LIR_CALL)
            continue;
        if (ins->nargs <= ins->call_nreg)
            continue;
        int out = 8 * (ins->nargs - ins->call_nreg);
        if (out > max_out)
            max_out = out;
      }
    }
    return max_out;
}

int lir_new_vreg(LirFn *fn)
{
    return fn->nvreg++;
}

int lir_new_label(LirFn *fn)
{
    int label = fn->label_count++;
    if (fn->label_count > fn->label_blocks_cap) {
        int old_cap = fn->label_blocks_cap;
        int new_cap = old_cap ? old_cap * 2 : 16;
        while (new_cap < fn->label_count)
            new_cap *= 2;
        LirBlockId *n = arena_alloc((size_t)new_cap * sizeof(*n));
        for (int i = 0; i < new_cap; i++)
            n[i] = LIR_NO_BLOCK;
        if (fn->label_blocks)
            memcpy(n, fn->label_blocks, (size_t)old_cap * sizeof(*n));
        fn->label_blocks = n;
        fn->label_blocks_cap = new_cap;
    }
    fn->label_blocks[label] = lir_new_block(fn);
    return label;
}

LirBlockId lir_label_block(LirFn *fn, int label)
{
    assert(label >= 0 && label < fn->label_count);
    assert(fn->label_blocks[label] != LIR_NO_BLOCK);
    return fn->label_blocks[label];
}

int lir_emit(LirFn *fn, Instr ins)
{
    if (fn->ninstr >= fn->cap) {
        fn->cap *= 2;
        Instr *n = arena_alloc((size_t)fn->cap * sizeof(*n));
        memcpy(n, fn->instrs, (size_t)fn->ninstr * sizeof(*n));
        fn->instrs = n;
    }
    int idx = fn->ninstr++;
    fn->instrs[idx] = ins;
    return idx;
}

static const char *op_name(LirOp op)
{
    switch (op) {
    case LIR_MOVI:  return "movi";
    case LIR_MOV:   return "mov";
    case LIR_LOAD:  return "load";
    case LIR_STORE: return "store";
    case LIR_LEA:   return "lea";
    case LIR_LEA_SYM: return "lea_sym";
    case LIR_ADD:   return "add";
    case LIR_SUB:   return "sub";
    case LIR_MUL:   return "mul";
    case LIR_DIV:   return "div";
    case LIR_MOD:   return "mod";
    case LIR_SDIV_POW2: return "sdiv_pow2";
    case LIR_SMOD_POW2: return "smod_pow2";
    case LIR_UDIV_POW2: return "udiv_pow2";
    case LIR_UMOD_POW2: return "umod_pow2";
    case LIR_AND:   return "and";
    case LIR_XOR:   return "xor";
    case LIR_OR:    return "or";
    case LIR_SHL:   return "shl";
    case LIR_SHR:   return "shr";
    case LIR_SAR:   return "sar";
    case LIR_NEG:   return "neg";
    case LIR_SETCC: return "setcc";
    case LIR_BR:    return "br";
    case LIR_JMP:   return "jmp";
    case LIR_LABEL: return "label";
    case LIR_CONV:  return "conv";
    case LIR_CALL:  return "call";
    case LIR_RET:   return "ret";
    case LIR_MEMCPY: return "memcpy";
    }
    return "?";
}

static const char *cc_name(LirCond cc)
{
    switch (cc) {
    case CC_EQ: return "eq";
    case CC_NE: return "ne";
    case CC_LT: return "lt";
    case CC_LE: return "le";
    case CC_GT: return "gt";
    case CC_GE: return "ge";
    }
    return "?";
}

static const char *conv_name(ConvKind k)
{
    switch (k) {
    case CONV_ZEXT8:     return "zext8";
    case CONV_SEXT8:     return "sext8";
    case CONV_ZEXT16:    return "zext16";
    case CONV_SEXT16:    return "sext16";
    case CONV_ZEXT32:    return "zext32";
    case CONV_SEXT32_64: return "sext32_64";
    case CONV_TRUNC_LO32: return "trunc_lo32";
    }
    return "?";
}

static void dump_operand(FILE *out, Operand o)
{
    switch (o.kind) {
    case OPND_NONE:
        fprintf(out, "_");
        return;
    case OPND_VREG:
        fprintf(out, "v%d", o.u.vreg);
        return;
    case OPND_PHYS:
        fprintf(out, "p%d", o.u.phys);
        return;
    case OPND_IMM:
        fprintf(out, "%ld", o.u.imm);
        return;
    case OPND_MEM:
        fprintf(out, "[");
        if (o.u.mem.base == LIR_FP)
            fprintf(out, "fp");
        else
            fprintf(out, "v%d", o.u.mem.base);
        if (o.u.mem.index != LIR_NO_IDX)
            fprintf(out, "+v%d*%d", o.u.mem.index, o.u.mem.scale);
        if (o.u.mem.disp)
            fprintf(out, "%+ld", o.u.mem.disp);
        fprintf(out, "]");
        return;
    }
}

static void dump_instr(FILE *out, Instr *ins, int index)
{
        fprintf(out, "    %3d  %-10s", index, op_name(ins->op));
        if (ins->dst != LIR_NO_VREG)
            fprintf(out, "v%d = ", ins->dst);
        switch (ins->op) {
        case LIR_MOVI:
            dump_operand(out, ins->a);
            break;
        case LIR_MOV:
        case LIR_LOAD:
        case LIR_NEG:
        case LIR_CONV:
            dump_operand(out, ins->a);
            if (ins->op == LIR_LOAD)
                fprintf(out, " %s", ins->sgn == LIR_SGN_Z ? "z" : "s");
            if (ins->op == LIR_CONV)
                fprintf(out, " %s", conv_name(ins->conv));
            break;
        case LIR_STORE:
            dump_operand(out, ins->a);
            fprintf(out, ", ");
            dump_operand(out, ins->b);
            break;
        case LIR_LEA:
            dump_operand(out, ins->a);
            break;
        case LIR_LEA_SYM:
            fprintf(out, "%s", ins->sym_name ? ins->sym_name : "?");
            break;
        case LIR_ADD:
        case LIR_SUB:
        case LIR_MUL:
        case LIR_DIV:
        case LIR_MOD:
        case LIR_SDIV_POW2:
        case LIR_SMOD_POW2:
        case LIR_UDIV_POW2:
        case LIR_UMOD_POW2:
        case LIR_AND:
        case LIR_XOR:
        case LIR_OR:
        case LIR_SHL:
        case LIR_SHR:
        case LIR_SAR:
        case LIR_SETCC:
            dump_operand(out, ins->a);
            if (ins->op == LIR_SDIV_POW2 || ins->op == LIR_SMOD_POW2 ||
                ins->op == LIR_UDIV_POW2 || ins->op == LIR_UMOD_POW2) {
                fprintf(out, " 2^%d %c", ins->aux,
                        ins->w == LIR_W4 ? '4' : '8');
            } else {
                fprintf(out, ", ");
                dump_operand(out, ins->b);
                fprintf(out, " %c", ins->w == LIR_W4 ? '4' : '8');
            }
            if (ins->op == LIR_SETCC)
                fprintf(out, " %s", cc_name(ins->cc));
            break;
        case LIR_BR:
            fprintf(out, "%s ", cc_name(ins->cc));
            dump_operand(out, ins->a);
            fprintf(out, ", ");
            dump_operand(out, ins->b);
            fprintf(out, " %c -> L%d", ins->w == LIR_W4 ? '4' : '8', ins->label);
            break;
        case LIR_JMP:
        case LIR_LABEL:
            fprintf(out, "L%d", ins->label);
            break;
        case LIR_CALL:
            if (ins->call_indirect)
                fprintf(out, "*v%d(", ins->call_reg);
            else
                fprintf(out, "%s(", ins->call_name);
            for (int a = 0; a < ins->nargs; a++) {
                if (a)
                    fprintf(out, ", ");
                dump_operand(out, ins->call_args[a]);
            }
            fprintf(out, ")");
            break;
        case LIR_RET:
            dump_operand(out, ins->a);
            break;
        case LIR_MEMCPY:
            dump_operand(out, ins->a);
            fprintf(out, ", ");
            dump_operand(out, ins->b);
            fprintf(out, " size=%d", ins->aux);
            break;
        }
        fprintf(out, "\n");
}

static void dump_block_term(FILE *out, const LirTerminator *term)
{
    switch (term->kind) {
    case LIR_TERM_NONE:
        fprintf(out, "    <unterminated>\n");
        return;
    case LIR_TERM_JMP:
        fprintf(out, "    jump       bb%d\n", term->target);
        return;
    case LIR_TERM_BR:
        fprintf(out, "    branch.%s.%c ", cc_name(term->cc),
                term->w == LIR_W4 ? 'i' : 'l');
        dump_operand(out, term->a);
        fprintf(out, ", ");
        dump_operand(out, term->b);
        fprintf(out, " ? bb%d : bb%d\n",
                term->true_target, term->false_target);
        return;
    case LIR_TERM_RET:
        fprintf(out, "    return      ");
        dump_operand(out, term->a);
        fprintf(out, "\n");
        return;
    }
}

void lir_dump_fn(LirFn *fn, FILE *out)
{
    fprintf(out, "function %s {  // %d blocks, %d vregs\n",
            fn->name, fn->nblocks, fn->nvreg);
    for (int b = 0; b < fn->nblocks; b++) {
        LirBlock *block = &fn->blocks[b];

        fprintf(out, "\n  bb%d", b);
        if (b == fn->entry_block)
            fprintf(out, " [entry]");
        fprintf(out, "  // preds:");
        if (block->npreds == 0)
            fprintf(out, " none");
        for (int p = 0; p < block->npreds; p++)
            fprintf(out, " bb%d", block->preds[p]);
        fprintf(out, "\n");

        for (int p = 0; p < block->nphis; p++) {
            LirPhi *phi = &block->phis[p];
            fprintf(out, "         v%d = phi ", phi->dst);
            for (int a = 0; a < phi->ninputs; a++) {
                if (a)
                    fprintf(out, ", ");
                fprintf(out, "[bb%d: v%d]",
                        phi->inputs[a].pred, phi->inputs[a].value);
            }
            fprintf(out, "\n");
        }
        for (int i = 0; i < block->ninstr; i++)
            dump_instr(out, &block->instrs[i], i);
        dump_block_term(out, &block->term);
    }
    fprintf(out, "}\n");
}
