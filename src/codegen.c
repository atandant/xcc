/* SPDX-License-Identifier: MIT */
#include "codegen.h"
#include "type.h"

#include <assert.h>
#include <limits.h>
#include <string.h>

/* Stack-machine code generator: expression results live in %rax, and complex
 * sub-expressions are spilled to reserved frame slots (Option B). Unlike a
 * push/pop emitter, %rsp never moves inside the function body: sema reserves a
 * temp area and an outgoing-argument area up front, so every `call` site is
 * already 16-byte aligned per the System V ABI.
 *
 * Note: char uses byte-wide load/store (movzbl/movb); short uses signed
 * 16-bit load/store (movswl/movw). int and pointers use 4/8-byte paths. */

static FILE *o;
static const char *fname;
static int labelseq;

static int cur_locals_size; /* bytes of locals + spilled reg params */
static int depth;           /* current expression-temp depth        */

/* System V integer argument registers, in order. */
static const char *argreg64[6] = {
    "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"
};
static const char *argreg32[6] = {
    "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d"
};
static const char *argreg16[6] = {
    "%di", "%si", "%dx", "%cx", "%r8w", "%r9w"
};
static const char *argreg8[6] = {
    "%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b"
};

static void gen_expr(Node *n);
static void gen_stmt(Node *n);

/* Offset of expression temp slot `i`, just below the locals area. */
static int temp_off(int i)
{
    return -(cur_locals_size + 8 * (i + 1));
}

/* Spill %rax into the next temp slot (replaces `push %rax`). */
static void spill(void)
{
    fprintf(o, "  mov %%rax, %d(%%rbp)\n", temp_off(depth));
    depth++;
}

/* Reload the most recently spilled temp (replaces `pop reg`). */
static void reload(const char *reg)
{
    depth--;
    fprintf(o, "  mov %d(%%rbp), %s\n", temp_off(depth), reg);
}

static int is_imm(Node *n)
{
    return n->kind == ND_NUM;
}

static int is_local(Node *n)
{
    return n->kind == ND_VAR;
}

static int is_int_local(Node *n)
{
    return is_local(n) && n->ty && type_is_integer(n->ty);
}

static int is_zero(Node *n)
{
    return is_imm(n) && n->val == 0;
}

static int is_direct_arg(Node *n)
{
    return is_imm(n) || is_local(n);
}

static int is_call_arg_direct(Node *n, int complex_after)
{
    return is_imm(n) || (is_local(n) && complex_after == 0);
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

/* Bytes of the in-memory representation for a scalar: 1 (char), 2 (short),
 * 4 (int), 8 (long / pointer). Drives every width-aware load/store below. */
static int scalar_width(Type *ty)
{
    if (type_is_integer(ty))
        return type_int_width(ty);   /* 1, 2, 4, or 8 */
    return 8;                        /* pointers (and any other scalar) */
}

static void emit_cltq(void);

static void emit_truncate_to_short(void)
{
    fprintf(o, "  movswl %%ax, %%eax\n");
    emit_cltq();
}

static void emit_load_slot(Type *ty, int offset)
{
    switch (scalar_width(ty)) {
    case 1:
        fprintf(o, "  movzbl %d(%%rbp), %%eax\n", offset);
        return;
    case 2:
        fprintf(o, "  movswl %d(%%rbp), %%eax\n", offset);
        emit_cltq();
        return;
    case 4:
        fprintf(o, "  movslq %d(%%rbp), %%rax\n", offset);
        return;
    default:
        fprintf(o, "  mov %d(%%rbp), %%rax\n", offset);
        return;
    }
}

static void emit_store_slot(Type *ty, int offset)
{
    switch (scalar_width(ty)) {
    case 1:
        fprintf(o, "  mov %%al, %d(%%rbp)\n", offset);
        return;
    case 2:
        fprintf(o, "  mov %%ax, %d(%%rbp)\n", offset);
        return;
    case 4:
        fprintf(o, "  mov %%eax, %d(%%rbp)\n", offset);
        return;
    default:
        fprintf(o, "  mov %%rax, %d(%%rbp)\n", offset);
        return;
    }
}

static void emit_var_rvalue(Node *n, const char *reg)
{
    assert(n->kind == ND_VAR);
    if (n->var_decay)
        fprintf(o, "  lea %d(%%rbp), %s\n", n->offset, reg);
    else {
        emit_load_slot(n->ty, n->offset);
        if (strcmp(reg, "%rax") != 0)
            fprintf(o, "  mov %%rax, %s\n", reg);
    }
}

static void emit_load_local(Node *n)
{
    assert(n->kind == ND_VAR);
    emit_var_rvalue(n, "%rax");
}

static int argreg_index(const char *reg64)
{
    for (int i = 0; i < 6; i++)
        if (strcmp(reg64, argreg64[i]) == 0)
            return i;
    return 0;
}

static void emit_reg_to_slot(const char *reg64, Type *ty, int offset)
{
    int i = argreg_index(reg64);

    switch (scalar_width(ty)) {
    case 1:
        fprintf(o, "  mov %s, %d(%%rbp)\n", argreg8[i], offset);
        return;
    case 2:
        fprintf(o, "  mov %s, %d(%%rbp)\n", argreg16[i], offset);
        return;
    case 4:
        fprintf(o, "  mov %s, %d(%%rbp)\n", argreg32[i], offset);
        return;
    default:
        fprintf(o, "  mov %s, %d(%%rbp)\n", reg64, offset);
        return;
    }
}

static void emit_arg_to_reg(Node *n, const char *reg)
{
    switch (n->kind) {
    case ND_NUM:
        fprintf(o, "  mov $%ld, %s\n", n->val, reg);
        return;
    case ND_VAR:
        emit_var_rvalue(n, reg);
        return;
    default:
        assert(0 && "not a direct argument");
    }
}

static void emit_arg_to_stack(Node *n, int off)
{
    switch (n->kind) {
    case ND_NUM:
        emit_load_imm(n->val);
        fprintf(o, "  mov %%rax, %d(%%rsp)\n", off);
        return;
    case ND_VAR:
        emit_var_rvalue(n, "%rax");
        fprintf(o, "  mov %%rax, %d(%%rsp)\n", off);
        return;
    default:
        assert(0 && "not a direct argument");
    }
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

/* Typed memory load/store at address in %rax; char is unsigned (movzbl). */
static void emit_load(Type *ty)
{
    switch (scalar_width(ty)) {
    case 1:
        fprintf(o, "  movzbl (%%rax), %%eax\n");
        return;
    case 2:
        fprintf(o, "  movswl (%%rax), %%eax\n");
        emit_cltq();
        return;
    case 4:
        fprintf(o, "  movslq (%%rax), %%rax\n");
        return;
    default:
        fprintf(o, "  mov (%%rax), %%rax\n");
        return;
    }
}

/* Store value register to address in %rdi (assignment / store through ptr). */
static void emit_store(Type *ty)
{
    switch (scalar_width(ty)) {
    case 1:
        fprintf(o, "  mov %%al, (%%rdi)\n");
        return;
    case 2:
        fprintf(o, "  mov %%ax, (%%rdi)\n");
        return;
    case 4:
        fprintf(o, "  mov %%eax, (%%rdi)\n");
        return;
    default:
        fprintf(o, "  mov %%rax, (%%rdi)\n");
        return;
    }
}

static void gen_addr(Node *n)
{
    switch (n->kind) {
    case ND_VAR:
        fprintf(o, "  lea %d(%%rbp), %%rax\n", n->offset);
        return;
    case ND_DEREF:
        /* The address of *p is just the value of p. */
        gen_expr(n->operand);
        return;
    default:
        /* sema rejects non-lvalue assignments. */
        assert(0 && "invalid lvalue");
    }
}

static void gen_call(Node *n)
{
    enum { MAX_CALL_ARGS = 4096 };
    int temp_slot[MAX_CALL_ARGS];
    int base = depth;
    int i = 0;
    int ntemps = 0;
    int complex_after = 0;

    assert(n->nargs <= MAX_CALL_ARGS);

    for (Node *a = n->args; a; a = a->next)
        if (!is_direct_arg(a))
            complex_after++;

    /* Only complex arguments need to be stashed before the call. Immediates
     * can be copied directly into final argument locations. Locals can too,
     * but only after any later complex argument that might mutate them. */
    for (Node *a = n->args; a; a = a->next, i++) {
        if (is_call_arg_direct(a, complex_after)) {
            temp_slot[i] = -1;
            continue;
        }

        gen_expr(a);
        temp_slot[i] = base + ntemps;
        fprintf(o, "  mov %%rax, %d(%%rbp)\n", temp_off(temp_slot[i]));
        depth++;
        ntemps++;
        if (!is_direct_arg(a))
            complex_after--;
    }

    /* Arguments 7+ go in the outgoing-arg area at 0(%rsp), 8(%rsp), ... */
    for (i = 6; i < n->nargs; i++) {
        if (temp_slot[i] < 0) {
            Node *a = n->args;
            for (int j = 0; j < i; j++)
                a = a->next;
            emit_arg_to_stack(a, 8 * (i - 6));
            continue;
        }
        fprintf(o, "  mov %d(%%rbp), %%rax\n", temp_off(temp_slot[i]));
        fprintf(o, "  mov %%rax, %d(%%rsp)\n", 8 * (i - 6));
    }

    /* Load the register arguments last, so nested calls above can't clobber
     * them before the call. */
    int nreg = n->nargs < 6 ? n->nargs : 6;
    Node *a = n->args;
    for (i = 0; i < nreg; i++, a = a->next) {
        if (temp_slot[i] < 0)
            emit_arg_to_reg(a, argreg64[i]);
        else
            fprintf(o, "  mov %d(%%rbp), %s\n",
                    temp_off(temp_slot[i]), argreg64[i]);
    }

    depth = base;

    /* SysV: %al = number of vector registers used (always 0 here). Required
     * for variadic and unprototyped callees. */
    fprintf(o, "  mov $0, %%al\n");
    fprintf(o, "  call %s\n", n->name);
    /* return value is already in %rax */
}

static int ptr_elem_size(Type *ptr_ty)
{
    Type *elem = type_ptr_elem(ptr_ty);
    return elem ? type_size(elem) : 1;
}

static int is_ptr_int_arith(BinOp op, Node *lhs, Node *rhs)
{
    if (op == OP_ADD)
        return (type_is_pointer(lhs->ty) && type_is_integer(rhs->ty)) ||
               (type_is_integer(lhs->ty) && type_is_pointer(rhs->ty));
    if (op == OP_SUB)
        return type_is_pointer(lhs->ty) && type_is_integer(rhs->ty);
    return 0;
}

static void gen_ptr_int_arith(BinOp op, Node *lhs, Node *rhs)
{
    Node *ptr = type_is_pointer(lhs->ty) ? lhs : rhs;
    Node *idx = type_is_pointer(lhs->ty) ? rhs : lhs;
    int scale = ptr_elem_size(ptr->ty);

    gen_expr(idx);
    if (scale > 1)
        fprintf(o, "  imul $%d, %%rax\n", scale);
    spill();
    gen_expr(ptr);
    reload("%rdi");
    if (op == OP_ADD)
        fprintf(o, "  add %%rdi, %%rax\n");
    else
        fprintf(o, "  sub %%rdi, %%rax\n");
}

static void gen_ptr_diff(Node *lhs, Node *rhs)
{
    int scale = ptr_elem_size(lhs->ty);

    gen_expr(rhs);
    spill();
    gen_expr(lhs);
    reload("%rdi");
    fprintf(o, "  sub %%rdi, %%rax\n");
    if (scale > 1) {
        fprintf(o, "  mov $%d, %%rcx\n", scale);
        fprintf(o, "  cqo\n");
        fprintf(o, "  idiv %%rcx\n");
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

static void emit_cltq(void)
{
    fprintf(o, "  cltq\n");
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
            fprintf(o, "  add $%ld, %%eax\n", rhs->val);
            emit_cltq();
            return 1;
        case OP_SUB:
            fprintf(o, "  sub $%ld, %%eax\n", rhs->val);
            emit_cltq();
            return 1;
        case OP_MUL:
            fprintf(o, "  imul $%ld, %%eax\n", rhs->val);
            emit_cltq();
            return 1;
        case OP_EQ:
        case OP_NE:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            fprintf(o, "  cmp $%ld, %%eax\n", rhs->val);
            emit_setcc(op);
            return 1;
        default:
            return 0;
        }
    }

    if (is_int_local(rhs)) {
        gen_expr(lhs);
        switch (op) {
        case OP_ADD:
            fprintf(o, "  addl %d(%%rbp), %%eax\n", rhs->offset);
            emit_cltq();
            return 1;
        case OP_SUB:
            fprintf(o, "  subl %d(%%rbp), %%eax\n", rhs->offset);
            emit_cltq();
            return 1;
        case OP_MUL:
            fprintf(o, "  imull %d(%%rbp), %%eax\n", rhs->offset);
            emit_cltq();
            return 1;
        case OP_EQ:
        case OP_NE:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            fprintf(o, "  cmpl %d(%%rbp), %%eax\n", rhs->offset);
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
            fprintf(o, "  add $%ld, %%eax\n", lhs->val);
            emit_cltq();
            return 1;
        case OP_MUL:
            fprintf(o, "  imul $%ld, %%eax\n", lhs->val);
            emit_cltq();
            return 1;
        case OP_EQ:
        case OP_NE:
            fprintf(o, "  cmp $%ld, %%eax\n", lhs->val);
            emit_setcc(op);
            return 1;
        default:
            return 0;
        }
    }

    if (is_int_local(lhs)) {
        gen_expr(rhs);
        switch (op) {
        case OP_ADD:
            fprintf(o, "  addl %d(%%rbp), %%eax\n", lhs->offset);
            emit_cltq();
            return 1;
        case OP_MUL:
            fprintf(o, "  imull %d(%%rbp), %%eax\n", lhs->offset);
            emit_cltq();
            return 1;
        case OP_EQ:
        case OP_NE:
            fprintf(o, "  cmpl %d(%%rbp), %%eax\n", lhs->offset);
            emit_setcc(op);
            return 1;
        default:
            return 0;
        }
    }

    return 0;
}

static int needs_i64_binop(Node *lhs, Node *rhs)
{
    return (is_imm(lhs) && !fits_i32(lhs->val)) ||
           (is_imm(rhs) && !fits_i32(rhs->val));
}

/* Codegen width for an integer/pointer binop: 8 if either operand is a
 * pointer or a long, else 4. char/int both compute in the 32-bit path
 * (char is promoted to int). This decides 32-bit vs 64-bit lowering. */
static int binop_width(Node *lhs, Node *rhs)
{
    if (type_is_pointer(lhs->ty) || type_is_pointer(rhs->ty))
        return 8;
    if (type_is_long(lhs->ty) || type_is_long(rhs->ty))
        return 8;
    return 4;
}

static void gen_binop_slow(BinOp op, Node *lhs, Node *rhs, int w)
{
    int use32 = (w == 4) && !needs_i64_binop(lhs, rhs);

    gen_expr(rhs);
    spill();
    gen_expr(lhs);
    reload("%rdi");

    if (use32) {
        switch (op) {
        case OP_ADD:
            fprintf(o, "  add %%edi, %%eax\n");
            emit_cltq();
            return;
        case OP_SUB:
            fprintf(o, "  sub %%edi, %%eax\n");
            emit_cltq();
            return;
        case OP_MUL:
            fprintf(o, "  imul %%edi, %%eax\n");
            emit_cltq();
            return;
        case OP_DIV:
            fprintf(o, "  cdq\n");
            fprintf(o, "  idiv %%edi\n");
            emit_cltq();
            return;
        case OP_MOD:
            fprintf(o, "  cdq\n");
            fprintf(o, "  idiv %%edi\n");
            fprintf(o, "  movslq %%edx, %%rax\n");
            return;
        case OP_EQ:
        case OP_NE:
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE:
            fprintf(o, "  cmp %%edi, %%eax\n");
            emit_setcc(op);
            return;
        }
    }

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
    if (type_is_pointer(lhs->ty) && type_is_pointer(rhs->ty) && op == OP_SUB) {
        gen_ptr_diff(lhs, rhs);
        return;
    }

    if (is_ptr_int_arith(op, lhs, rhs)) {
        gen_ptr_int_arith(op, lhs, rhs);
        return;
    }

    if (fold_binop(op, lhs, rhs))
        return;

    int w = binop_width(lhs, rhs);

    if (is_cmp(op) && is_zero(rhs)) {
        gen_expr(lhs);
        if (w == 4)
            fprintf(o, "  cmpl $0, %%eax\n");
        else
            fprintf(o, "  cmp $0, %%rax\n");
        emit_setcc(op);
        return;
    }

    /* The imm/local fast paths emit 32-bit ops; only use them at width 4. */
    if (w == 4) {
        if (gen_binop_fast_rhs(op, lhs, rhs))
            return;

        if (gen_binop_commuted(op, lhs, rhs))
            return;
    }

    gen_binop_slow(op, lhs, rhs, w);
}

static void gen_expr(Node *n)
{
    /* A decayed array's rvalue is its address, not a loaded value. This
     * covers both `a` (lea) and the intermediate `*(a + i)` of a multi-dim
     * index (the pointer itself); gen_addr handles ND_VAR and ND_DEREF. */
    if (n->var_decay) {
        gen_addr(n);
        return;
    }

    switch (n->kind) {
    case ND_NUM:
        emit_load_imm(n->val);
        return;
    case ND_VAR:
        emit_load_local(n);
        return;
    case ND_CALL:
        gen_call(n);
        return;
    case ND_NEG:
        gen_expr(n->operand);
        fprintf(o, "  neg %%rax\n");
        return;
    case ND_ADDR:
        gen_addr(n->operand);
        return;
    case ND_DEREF:
        gen_addr(n);              /* %rax = address */
        emit_load(n->ty);         /* load value by result type */
        return;
    case ND_CAST:
        gen_expr(n->operand);
        /* void: value discarded. char: truncate to the unsigned-byte model
         * (required, and observable after promotion). ptr->int: normalize to
         * the sign-extended int-in-%rax invariant (impl-defined value, kept
         * for hygiene). All other conversions are representation no-ops. */
        if (type_is_void(n->ty))
            ;
        else if (type_is_char(n->ty) && !type_is_char(n->operand->ty))
            fprintf(o, "  movzbl %%al, %%eax\n");
        else if (type_is_short(n->ty) && !type_is_short(n->operand->ty))
            emit_truncate_to_short();
        else if (type_is_long(n->ty) && type_is_integer(n->operand->ty) &&
                 type_int_width(n->operand->ty) == 4)
            emit_cltq();
        else if (type_is_integer(n->ty) && type_int_width(n->ty) == 4 &&
                 (type_is_pointer(n->operand->ty) ||
                  (type_is_integer(n->operand->ty) &&
                   type_int_width(n->operand->ty) == 8)))
            /* Narrow a 64-bit value (pointer or long) to a 32-bit int,
             * re-establishing the sign-extended-in-%rax invariant. A cast to
             * long keeps the full %rax (D10). */
            fprintf(o, "  movslq %%eax, %%rax\n");
        return;
    case ND_ASSIGN:
        gen_addr(n->lhs);
        spill();
        gen_expr(n->rhs);
        reload("%rdi");           /* %rdi = address, %rax = value */
        emit_store(n->lhs->ty);
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
        if (n->operand)
            gen_expr(n->operand);
        fprintf(o, "  jmp .L.return.%s\n", fname);
        return;
    case ND_EXPR_STMT:
        gen_expr(n->operand);
        return;
    case ND_DECL:
        if (n->init) {
            gen_expr(n->init);
            emit_store_slot(n->ty, n->offset);
        }
        return;
    case ND_IF: {
        int id = labelseq++;
        gen_expr(n->cond);
        fprintf(o, "  test %%rax, %%rax\n");
        if (n->else_body) {
            fprintf(o, "  je .L.else.%s.%d\n", fname, id);
            gen_stmt(n->then_body);
            fprintf(o, "  jmp .L.end.%s.%d\n", fname, id);
            fprintf(o, ".L.else.%s.%d:\n", fname, id);
            gen_stmt(n->else_body);
            fprintf(o, ".L.end.%s.%d:\n", fname, id);
        } else {
            fprintf(o, "  je .L.end.%s.%d\n", fname, id);
            gen_stmt(n->then_body);
            fprintf(o, ".L.end.%s.%d:\n", fname, id);
        }
        return;
    }
    case ND_WHILE: {
        int id = labelseq++;
        fprintf(o, ".L.begin.%s.%d:\n", fname, id);
        gen_expr(n->cond);
        fprintf(o, "  test %%rax, %%rax\n");
        fprintf(o, "  je .L.end.%s.%d\n", fname, id);
        gen_stmt(n->then_body);
        fprintf(o, "  jmp .L.begin.%s.%d\n", fname, id);
        fprintf(o, ".L.end.%s.%d:\n", fname, id);
        return;
    }
    case ND_FOR: {
        int id = labelseq++;
        if (n->init)
            gen_expr(n->init);
        fprintf(o, ".L.begin.%s.%d:\n", fname, id);
        if (n->cond) {
            gen_expr(n->cond);
            fprintf(o, "  test %%rax, %%rax\n");
            fprintf(o, "  je .L.end.%s.%d\n", fname, id);
        }
        gen_stmt(n->then_body);
        if (n->step)
            gen_expr(n->step);
        fprintf(o, "  jmp .L.begin.%s.%d\n", fname, id);
        fprintf(o, ".L.end.%s.%d:\n", fname, id);
        return;
    }
    case ND_BLOCK:
        for (Node *s = n->body; s; s = s->next)
            gen_stmt(s);
        return;
    default:
        return;
    }
}

static int stmt_returns(Node *n);

static int stmt_list_returns(Node *body)
{
    for (Node *s = body; s; s = s->next)
        if (stmt_returns(s))
            return 1;
    return 0;
}

static int stmt_returns(Node *n)
{
    switch (n->kind) {
    case ND_RETURN:
        return 1;
    case ND_IF:
        return n->else_body &&
               stmt_returns(n->then_body) &&
               stmt_returns(n->else_body);
    case ND_BLOCK:
        return stmt_list_returns(n->body);
    default:
        return 0;
    }
}

static void gen_function(Function *fn)
{
    if (!fn->is_definition)
        return;

    fname = fn->name;
    labelseq = 0;
    cur_locals_size = fn->locals_size;
    depth = 0;

    fprintf(o, "  .globl %s\n", fn->name);
    fprintf(o, "%s:\n", fn->name);

    /* prologue */
    fprintf(o, "  push %%rbp\n");
    fprintf(o, "  mov %%rsp, %%rbp\n");
    if (fn->stack_size)
        fprintf(o, "  sub $%d, %%rsp\n", fn->stack_size);

    /* spill the register-passed parameters into their slots */
    int i = 0;
    for (Param *p = fn->params; p; p = p->next, i++) {
        if (i < 6)
            emit_reg_to_slot(argreg64[i], type_decay(p->ty), p->offset);
    }

    for (Node *s = fn->body; s; s = s->next)
        gen_stmt(s);

    if (!stmt_list_returns(fn->body))
        emit_zero_rax();

    /* epilogue */
    fprintf(o, ".L.return.%s:\n", fn->name);
    fprintf(o, "  leave\n");
    fprintf(o, "  ret\n");
}

void codegen(Function *prog, FILE *out)
{
    o = out;
    fprintf(o, "  .text\n");
    for (Function *fn = prog; fn; fn = fn->next)
        gen_function(fn);
}
