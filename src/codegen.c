/* SPDX-License-Identifier: MIT */
#include "codegen.h"

/* A simple stack-machine code generator: every expression leaves its
 * result in %rax. Binary operands are spilled to the hardware stack.
 * Note (known v0.0.1 deviation): C `int` is 32-bit on this ABI, but we
 * compute in 64-bit registers/slots throughout. The exit-status tests only
 * observe the low 8 bits, so this is invisible for now and fixed when the
 * real type system lands. */

static FILE *o;
static const char *fname;

static void gen_expr(Node *n)
{
    switch (n->kind) {
    case ND_NUM:
        fprintf(o, "  mov $%ld, %%rax\n", n->val);
        return;
    case ND_VAR:
        fprintf(o, "  mov %d(%%rbp), %%rax\n", n->offset);
        return;
    case ND_NEG:
        gen_expr(n->operand);
        fprintf(o, "  neg %%rax\n");
        return;
    case ND_ASSIGN:
        /* sema guarantees lhs is ND_VAR */
        gen_expr(n->rhs);
        fprintf(o, "  mov %%rax, %d(%%rbp)\n", n->lhs->offset);
        return;
    case ND_BINOP:
        gen_expr(n->rhs);
        fprintf(o, "  push %%rax\n");
        gen_expr(n->lhs);
        fprintf(o, "  pop %%rdi\n"); /* %rax = lhs, %rdi = rhs */
        switch (n->op) {
        case OP_ADD:
            fprintf(o, "  add %%rdi, %%rax\n");
            break;
        case OP_SUB:
            fprintf(o, "  sub %%rdi, %%rax\n");
            break;
        case OP_MUL:
            fprintf(o, "  imul %%rdi, %%rax\n");
            break;
        case OP_DIV:
            fprintf(o, "  cqo\n");
            fprintf(o, "  idiv %%rdi\n");
            break;
        case OP_MOD:
            fprintf(o, "  cqo\n");
            fprintf(o, "  idiv %%rdi\n");
            fprintf(o, "  mov %%rdx, %%rax\n");
            break;
        case OP_EQ:
        case OP_NE:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE: {
            const char *cc;
            switch (n->op) {
            case OP_EQ: cc = "sete"; break;
            case OP_NE: cc = "setne"; break;
            case OP_LT: cc = "setl"; break;
            case OP_LE: cc = "setle"; break;
            case OP_GT: cc = "setg"; break;
            default:    cc = "setge"; break;
            }
            fprintf(o, "  cmp %%rdi, %%rax\n");
            fprintf(o, "  %s %%al\n", cc);
            fprintf(o, "  movzbq %%al, %%rax\n");
            break;
        }
        }
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
    fprintf(o, "  mov $0, %%rax\n");

    /* epilogue */
    fprintf(o, ".L.return.%s:\n", fn->name);
    fprintf(o, "  mov %%rbp, %%rsp\n");
    fprintf(o, "  pop %%rbp\n");
    fprintf(o, "  ret\n");
}
