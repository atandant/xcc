/* SPDX-License-Identifier: MIT */
#include "codegen.h"

#include <assert.h>
#include <limits.h>

/* This is still intentionally a stack-machine code generator: expression
 * results live in %rax, and complex binary RHS values are spilled to the
 * hardware stack. The difference from the skeleton emitter is that simple
 * immediates and locals are consumed directly as x86 operands when legal.
 *
 * Note (known v0.0.1 deviation): C `int` is 32-bit on this ABI, but we
 * compute in 64-bit registers/slots throughout. The exit-status tests only
 * observe the low 8 bits, so this is invisible for now and fixed when the
 * real type system lands. */

static FILE *o;
static const char *fname;

static void gen_expr(Node *n);

static int is_imm(Node *n)
{
    return n->kind == ND_NUM;
}

static int is_local(Node *n)
{
    return n->kind == ND_VAR;
}

static int is_zero(Node *n)
{
    return is_imm(n) && n->val == 0;
}

static int fits_i32(long val)
{
    return val >= INT_MIN && val <= INT_MAX;
}

static void emit_zero_rax(void)
{
    fprintf(o, "  xor %%eax, %%eax\n");
}

static void emit_load_imm(long val)
{
    if (val == 0)
        emit_zero_rax();
    else
        fprintf(o, "  mov $%ld, %%rax\n", val);
}

static void emit_load_local(Node *n)
{
    assert(n->kind == ND_VAR);
    fprintf(o, "  mov %d(%%rbp), %%rax\n", n->offset);
}

static const char *setcc_for(BinOp op)
{
    switch (op) {
    case OP_EQ: return "sete";
    case OP_NE: return "setne";
    case OP_LT: return "setl";
    case OP_LE: return "setle";
    case OP_GT: return "setg";
    case OP_GE: return "setge";
    default:
        assert(0 && "not a comparison operator");
        return "sete";
    }
}

static int is_cmp(BinOp op)
{
    return op == OP_EQ || op == OP_NE ||
           op == OP_LT || op == OP_LE ||
           op == OP_GT || op == OP_GE;
}

static void emit_setcc(BinOp op)
{
    fprintf(o, "  %s %%al\n", setcc_for(op));
    fprintf(o, "  movzbq %%al, %%rax\n");
}

static void gen_addr(Node *n)
{
    switch (n->kind) {
    case ND_VAR:
        fprintf(o, "  lea %d(%%rbp), %%rax\n", n->offset);
        return;
    default:
        /* sema rejects non-lvalue assignments. */
        assert(0 && "invalid lvalue");
    }
}

static int fold_binop(BinOp op, Node *lhs, Node *rhs)
{
    if (is_zero(rhs)) {
        switch (op) {
        case OP_ADD:
        case OP_SUB:
            gen_expr(lhs);
            return 1;
        case OP_MUL:
            gen_expr(lhs);
            emit_zero_rax();
            return 1;
        default:
            break;
        }
    }

    if (is_imm(rhs) && rhs->val == 1) {
        switch (op) {
        case OP_MUL:
        case OP_DIV:
            gen_expr(lhs);
            return 1;
        case OP_MOD:
            gen_expr(lhs);
            emit_zero_rax();
            return 1;
        default:
            break;
        }
    }

    if (is_imm(lhs) && lhs->val == 0 && op == OP_ADD) {
        gen_expr(rhs);
        return 1;
    }

    if (is_imm(lhs) && lhs->val == 1 && op == OP_MUL) {
        gen_expr(rhs);
        return 1;
    }

    return 0;
}

static int gen_binop_fast_rhs(BinOp op, Node *lhs, Node *rhs)
{
    if (op == OP_DIV || op == OP_MOD)
        return 0;

    if (is_imm(rhs)) {
        if (!fits_i32(rhs->val))
            return 0;

        gen_expr(lhs);
        switch (op) {
        case OP_ADD:
            fprintf(o, "  add $%ld, %%rax\n", rhs->val);
            return 1;
        case OP_SUB:
            fprintf(o, "  sub $%ld, %%rax\n", rhs->val);
            return 1;
        case OP_MUL:
            fprintf(o, "  imul $%ld, %%rax\n", rhs->val);
            return 1;
        case OP_EQ:
        case OP_NE:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            fprintf(o, "  cmp $%ld, %%rax\n", rhs->val);
            emit_setcc(op);
            return 1;
        default:
            return 0;
        }
    }

    if (is_local(rhs)) {
        gen_expr(lhs);
        switch (op) {
        case OP_ADD:
            fprintf(o, "  add %d(%%rbp), %%rax\n", rhs->offset);
            return 1;
        case OP_SUB:
            fprintf(o, "  sub %d(%%rbp), %%rax\n", rhs->offset);
            return 1;
        case OP_MUL:
            fprintf(o, "  imul %d(%%rbp), %%rax\n", rhs->offset);
            return 1;
        case OP_EQ:
        case OP_NE:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            fprintf(o, "  cmp %d(%%rbp), %%rax\n", rhs->offset);
            emit_setcc(op);
            return 1;
        default:
            return 0;
        }
    }

    return 0;
}

static int gen_binop_commuted(BinOp op, Node *lhs, Node *rhs)
{
    if (!(op == OP_ADD || op == OP_MUL || op == OP_EQ || op == OP_NE))
        return 0;

    if (is_imm(lhs)) {
        if (!fits_i32(lhs->val))
            return 0;

        gen_expr(rhs);
        switch (op) {
        case OP_ADD:
            fprintf(o, "  add $%ld, %%rax\n", lhs->val);
            return 1;
        case OP_MUL:
            fprintf(o, "  imul $%ld, %%rax\n", lhs->val);
            return 1;
        case OP_EQ:
        case OP_NE:
            fprintf(o, "  cmp $%ld, %%rax\n", lhs->val);
            emit_setcc(op);
            return 1;
        default:
            return 0;
        }
    }

    if (is_local(lhs)) {
        gen_expr(rhs);
        switch (op) {
        case OP_ADD:
            fprintf(o, "  add %d(%%rbp), %%rax\n", lhs->offset);
            return 1;
        case OP_MUL:
            fprintf(o, "  imul %d(%%rbp), %%rax\n", lhs->offset);
            return 1;
        case OP_EQ:
        case OP_NE:
            fprintf(o, "  cmp %d(%%rbp), %%rax\n", lhs->offset);
            emit_setcc(op);
            return 1;
        default:
            return 0;
        }
    }

    return 0;
}

static void gen_binop_slow(BinOp op, Node *lhs, Node *rhs)
{
    gen_expr(rhs);
    fprintf(o, "  push %%rax\n");
    gen_expr(lhs);
    fprintf(o, "  pop %%rdi\n"); /* %rax = lhs, %rdi = rhs */

    switch (op) {
    case OP_ADD:
        fprintf(o, "  add %%rdi, %%rax\n");
        return;
    case OP_SUB:
        fprintf(o, "  sub %%rdi, %%rax\n");
        return;
    case OP_MUL:
        fprintf(o, "  imul %%rdi, %%rax\n");
        return;
    case OP_DIV:
        fprintf(o, "  cqo\n");
        fprintf(o, "  idiv %%rdi\n");
        return;
    case OP_MOD:
        fprintf(o, "  cqo\n");
        fprintf(o, "  idiv %%rdi\n");
        fprintf(o, "  mov %%rdx, %%rax\n");
        return;
    case OP_EQ:
    case OP_NE:
    case OP_LT:
    case OP_LE:
    case OP_GT:
    case OP_GE:
        fprintf(o, "  cmp %%rdi, %%rax\n");
        emit_setcc(op);
        return;
    }
}

static void gen_binop(BinOp op, Node *lhs, Node *rhs)
{
    if (fold_binop(op, lhs, rhs))
        return;

    if (is_cmp(op) && is_zero(rhs)) {
        gen_expr(lhs);
        fprintf(o, "  test %%rax, %%rax\n");
        emit_setcc(op);
        return;
    }

    if (gen_binop_fast_rhs(op, lhs, rhs))
        return;

    if (gen_binop_commuted(op, lhs, rhs))
        return;

    gen_binop_slow(op, lhs, rhs);
}

static void gen_expr(Node *n)
{
    switch (n->kind) {
    case ND_NUM:
        emit_load_imm(n->val);
        return;
    case ND_VAR:
        emit_load_local(n);
        return;
    case ND_NEG:
        gen_expr(n->operand);
        fprintf(o, "  neg %%rax\n");
        return;
    case ND_ASSIGN:
        gen_addr(n->lhs);
        fprintf(o, "  push %%rax\n");
        gen_expr(n->rhs);
        fprintf(o, "  pop %%rdi\n");
        fprintf(o, "  mov %%rax, (%%rdi)\n");
        return;
    case ND_BINOP:
        gen_binop(n->op, n->lhs, n->rhs);
        return;
    default:
        return;
    }
}

static void gen_stmt(Node *n)
{
    switch (n->kind) {
    case ND_RETURN:
        gen_expr(n->operand);
        fprintf(o, "  jmp .L.return.%s\n", fname);
        return;
    case ND_EXPR_STMT:
        gen_expr(n->operand);
        return;
    case ND_DECL:
        if (n->init) {
            gen_expr(n->init);
            fprintf(o, "  mov %%rax, %d(%%rbp)\n", n->offset);
        }
        return;
    default:
        return;
    }
}

void codegen(Function *fn, FILE *out)
{
    o = out;
    fname = fn->name;

    fprintf(o, "  .text\n");
    fprintf(o, "  .globl %s\n", fn->name);
    fprintf(o, "%s:\n", fn->name);

    /* prologue */
    fprintf(o, "  push %%rbp\n");
    fprintf(o, "  mov %%rsp, %%rbp\n");
    if (fn->stack_size)
        fprintf(o, "  sub $%d, %%rsp\n", fn->stack_size);

    for (Node *s = fn->body; s; s = s->next)
        gen_stmt(s);

    /* falling off the end returns 0 */
    emit_zero_rax();

    /* epilogue */
    fprintf(o, ".L.return.%s:\n", fn->name);
    fprintf(o, "  mov %%rbp, %%rsp\n");
    fprintf(o, "  pop %%rbp\n");
    fprintf(o, "  ret\n");
}
