/* SPDX-License-Identifier: MIT */
#include "sema_frame.h"

/* ---- frame sizing (must be an upper bound on what codegen spills) ----
 *
 * Mirrors codegen's generic (non-fast-path) lowering. Fast paths only ever
 * spill fewer temps, so this is always a safe over-estimate. */

static int cur_max_depth; /* max simultaneously-live expression temps      */
static int cur_max_out;   /* max outgoing stack-arg bytes over all calls    */

static int max_int(int a, int b) { return a > b ? a : b; }

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

static int is_cmp(BinOp op)
{
    return op == OP_EQ || op == OP_NE ||
           op == OP_LT || op == OP_LE ||
           op == OP_GT || op == OP_GE;
}

static int fits_i32(long val)
{
    return val >= -2147483647L - 1L && val <= 2147483647L;
}

static int is_direct_arg(Node *n)
{
    return is_imm(n) || is_local(n);
}

static int is_call_arg_direct(Node *n, int complex_after)
{
    return is_imm(n) || (is_local(n) && complex_after == 0);
}

/* Mirror codegen binop_width: 8 for pointer/long operands, else 4. */
static int measure_binop_width(Node *lhs, Node *rhs)
{
    if (!lhs || !rhs)
        return 4;
    if (type_is_pointer(lhs->ty) || type_is_pointer(rhs->ty))
        return 8;
    if (type_is_long(lhs->ty) || type_is_long(rhs->ty))
        return 8;
    return 4;
}

static int measure_expr(Node *e)
{
    if (!e)
        return 0;

    switch (e->kind) {
    case ND_NUM:
    case ND_VAR:
        return 0;
    case ND_NEG:
    case ND_ADDR:
    case ND_DEREF:
    case ND_CAST:
        return measure_expr(e->operand);
    case ND_BINOP: {
        int r = measure_expr(e->rhs);
        int l = measure_expr(e->lhs);
        int w = measure_binop_width(e->lhs, e->rhs);

        if (is_zero(e->rhs)) {
            switch (e->op) {
            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
                return l;
            default:
                break;
            }
        }

        if (is_imm(e->rhs) && e->rhs->val == 1) {
            switch (e->op) {
            case OP_MUL:
            case OP_DIV:
            case OP_MOD:
                return l;
            default:
                break;
            }
        }

        if (is_imm(e->lhs) && e->lhs->val == 0 && e->op == OP_ADD)
            return r;

        if (is_imm(e->lhs) && e->lhs->val == 1 && e->op == OP_MUL)
            return r;

        if (is_cmp(e->op) && is_zero(e->rhs))
            return l;

        /* 32-bit fast paths avoid a rhs spill; 64-bit lowering always spills. */
        if (w == 4 && e->op != OP_DIV && e->op != OP_MOD) {
            if (is_imm(e->rhs) && fits_i32(e->rhs->val))
                return l;
            if (is_local(e->rhs))
                return l;
        }

        if (w == 4 && (e->op == OP_ADD || e->op == OP_MUL ||
                       e->op == OP_EQ || e->op == OP_NE)) {
            if (is_imm(e->lhs) && fits_i32(e->lhs->val))
                return r;
            if (is_local(e->lhs))
                return r;
        }

        return max_int(r, 1 + l);   /* spill rhs, then evaluate lhs */
    }
    case ND_ASSIGN: {
        int r = measure_expr(e->rhs);
        int l = measure_expr(e->lhs);
        return max_int(l, 1 + r);   /* spill addr, then evaluate rhs */
    }
    case ND_CALL: {
        int peak = 0;
        int live = 0;
        int complex_after = 0;

        for (Node *a = e->args; a; a = a->next)
            if (!is_direct_arg(a))
                complex_after++;

        for (Node *a = e->args; a; a = a->next) {
            int direct = is_call_arg_direct(a, complex_after);

            peak = max_int(peak, live + measure_expr(a));
            if (!direct) {
                live++;
                peak = max_int(peak, live);
            }
            if (!is_direct_arg(a))
                complex_after--;
        }
        int stackargs = e->nargs > 6 ? e->nargs - 6 : 0;
        cur_max_out = max_int(cur_max_out, 8 * stackargs);
        return peak;
    }
    default:
        return 0;
    }
}

static void measure_stmt(Node *s);

static void measure_stmt_list(Node *body)
{
    for (Node *s = body; s; s = s->next)
        measure_stmt(s);
}

static void measure_stmt(Node *s)
{
    switch (s->kind) {
    case ND_DECL:
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->init));
        return;
    case ND_RETURN:
    case ND_EXPR_STMT:
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->operand));
        return;
    case ND_IF:
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->cond));
        measure_stmt(s->then_body);
        if (s->else_body)
            measure_stmt(s->else_body);
        return;
    case ND_WHILE:
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->cond));
        measure_stmt(s->then_body);
        return;
    case ND_FOR:
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->init));
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->cond));
        cur_max_depth = max_int(cur_max_depth, measure_expr(s->step));
        measure_stmt(s->then_body);
        return;
    case ND_BLOCK:
        measure_stmt_list(s->body);
        return;
    default:
        return;
    }
}

void sema_measure_frame(Node *body, int *out_max_depth, int *out_max_out)
{
    cur_max_depth = 0;
    cur_max_out = 0;
    measure_stmt_list(body);
    *out_max_depth = cur_max_depth;
    *out_max_out = cur_max_out;
}
