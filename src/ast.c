/* SPDX-License-Identifier: MIT */
#include "ast.h"
#include "arena.h"
#include "diag.h"
#include "intconst.h"
#include "sema_enum.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static Node *literal_head;
static Node *literal_tail;
static int next_literal_id;

void ast_reset(void)
{
    literal_head = NULL;
    literal_tail = NULL;
    next_literal_id = 0;
    g_typedef_decls = NULL;
}

DeclSpec *declspec_new(void)
{
    return arena_alloc_zeroed(sizeof(DeclSpec));
}

DeclSpec *declspec_add_storage(DeclSpec *spec, StorageClass storage,
                               SourceLoc loc)
{
    if (!spec)
        spec = declspec_new();
    if (spec->storage != STORAGE_NONE)
        diag_error_at(loc, "multiple storage classes in declaration specifiers");
    else
        spec->storage = storage;
    return spec;
}

DeclSpec *declspec_add_type(DeclSpec *spec, Type *ty, SourceLoc loc)
{
    if (!spec)
        spec = declspec_new();
    if (spec->named_type)
        diag_error_at(loc, "multiple data types in declaration specifiers");
    else
        spec->named_type = ty;
    return spec;
}

DeclSpec *declspec_add_qualifier(DeclSpec *spec, unsigned qualifier,
                                 SourceLoc loc)
{
    if (!spec)
        spec = declspec_new();
    if (qualifier == TQ_CONST) {
        if (++spec->nconst > 1)
            diag_error_at(loc, "duplicate 'const' type qualifier");
    } else if (qualifier == TQ_VOLATILE) {
        if (++spec->nvolatile > 1)
            diag_error_at(loc, "duplicate 'volatile' type qualifier");
    }
    return spec;
}

static unsigned declspec_qualifiers(const DeclSpec *spec)
{
    return (spec->nconst ? TQ_CONST : TQ_NONE) |
           (spec->nvolatile ? TQ_VOLATILE : TQ_NONE);
}

DeclSpec *declspec_add_builtin(DeclSpec *spec, TypeSpecKind kind,
                               SourceLoc loc)
{
    int *count = NULL;

    if (!spec)
        spec = declspec_new();
    switch (kind) {
    case TYPE_SPEC_VOID:     count = &spec->nvoid; break;
    case TYPE_SPEC_CHAR:     count = &spec->nchar; break;
    case TYPE_SPEC_SHORT:    count = &spec->nshort; break;
    case TYPE_SPEC_INT:      count = &spec->nint; break;
    case TYPE_SPEC_LONG:     count = &spec->nlong; break;
    case TYPE_SPEC_SIGNED:   count = &spec->nsigned; break;
    case TYPE_SPEC_UNSIGNED: count = &spec->nunsigned; break;
    case TYPE_SPEC_FLOAT:    count = &spec->nfloat; break;
    case TYPE_SPEC_DOUBLE:   count = &spec->ndouble; break;
    }
    if (++*count > 1)
        diag_error_at(loc, "duplicate type specifier in declaration");
    return spec;
}

Type *declspec_type(DeclSpec *spec, SourceLoc loc)
{
    int builtin;
    unsigned qualifiers;
    Type *ty;

    if (!spec)
        return type_int();
    builtin = spec->nvoid + spec->nchar + spec->nshort + spec->nint +
              spec->nlong + spec->nsigned + spec->nunsigned;
    builtin += spec->nfloat + spec->ndouble;
    qualifiers = declspec_qualifiers(spec);
    if (spec->named_type) {
        if (builtin)
            diag_error_at(loc, "invalid combination of type specifiers");
        ty = spec->named_type;
        if (spec->nconst && type_is_const(ty))
            diag_error_at(loc, "duplicate 'const' type qualifier");
        if (spec->nvolatile && type_is_volatile(ty))
            diag_error_at(loc, "duplicate 'volatile' type qualifier");
        if (qualifiers && type_unqualified(ty)->kind == TY_FUNC)
            diag_error_at(loc, qualifiers == TQ_CONST
                          ? "function type must not be const-qualified"
                          : qualifiers == TQ_VOLATILE
                          ? "function type must not be volatile-qualified"
                          : "function type must not be const-volatile-qualified");
        return type_qualify(ty, qualifiers);
    }
    if (spec->nsigned && spec->nunsigned) {
        diag_error_at(loc, "both signed and unsigned specified");
        return type_int();
    }
    if (spec->nfloat || spec->ndouble) {
        if (spec->nfloat && spec->ndouble)
            diag_error_at(loc, "both float and double specified");
        if (spec->nsigned || spec->nunsigned || spec->nshort || spec->nint ||
            spec->nchar || spec->nvoid)
            diag_error_at(loc, "invalid integer type specifier with floating type");
        if (spec->nlong) {
            if (!spec->ndouble)
                diag_error_at(loc, "invalid combination with 'float'");
        }
        ty = spec->nfloat ? type_float() :
             spec->nlong ? type_long_double() : type_double();
        return type_qualify(ty, qualifiers);
    }
    if (spec->nvoid) {
        if (builtin != 1)
            diag_error_at(loc, "invalid combination with 'void'");
        ty = type_void();
        return type_qualify(ty, qualifiers);
    }
    if (spec->nchar) {
        if (spec->nshort || spec->nint || spec->nlong)
            diag_error_at(loc, "invalid combination with 'char'");
        if (spec->nunsigned)
            ty = type_unsigned_char();
        else if (spec->nsigned)
            ty = type_signed_char();
        else
            ty = type_char();
        return type_qualify(ty, qualifiers);
    }
    if (spec->nshort && spec->nlong)
        diag_error_at(loc, "both short and long specified");
    if (spec->nshort)
        ty = spec->nunsigned ? type_unsigned_short() : type_short();
    else if (spec->nlong)
        ty = spec->nunsigned ? type_unsigned_long() : type_long();
    else
        ty = spec->nunsigned ? type_unsigned_int() : type_int();
    return type_qualify(ty, qualifiers);
}

static Node *new_node(NodeKind kind, SourceLoc loc)
{
    Node *n = arena_alloc_zeroed(sizeof(Node));
    n->kind = kind;
    n->loc = loc;
    return n;
}

Node *node_num(long v, SourceLoc loc)
{
    Node *n = new_node(ND_NUM, loc);
    n->val = v;
    return n;
}

Node *node_string(StringToken *token, SourceLoc loc)
{
    Node *n = new_node(ND_STRING, loc);
    char label[32];

    n->string_data = token->data;
    n->string_len = token->len;
    n->string_wide = token->wide;
    snprintf(label, sizeof(label), ".L.str.%d", next_literal_id++);
    n->string_label = arena_strdup(label);
    if (literal_tail)
        literal_tail->string_next = n;
    else
        literal_head = n;
    literal_tail = n;
    return n;
}

static uint32_t decode_utf8_unit(const uint32_t *bytes, int len, int *index,
                                 SourceLoc loc)
{
    uint32_t first = bytes[(*index)++];
    uint32_t value;
    int extra;

    if (first < 0x80)
        return first;
    if (first >= 0xc2 && first <= 0xdf) {
        value = first & 0x1f;
        extra = 1;
    } else if (first >= 0xe0 && first <= 0xef) {
        value = first & 0x0f;
        extra = 2;
    } else if (first >= 0xf0 && first <= 0xf4) {
        value = first & 0x07;
        extra = 3;
    } else {
        diag_error_at(loc, "invalid UTF-8 in character string literal");
        return 0xfffd;
    }
    if (*index + extra > len) {
        diag_error_at(loc, "incomplete UTF-8 in character string literal");
        *index = len;
        return 0xfffd;
    }
    for (int i = 0; i < extra; i++) {
        uint32_t next = bytes[(*index)++];
        if (next < 0x80 || next > 0xbf) {
            diag_error_at(loc, "invalid UTF-8 in character string literal");
            return 0xfffd;
        }
        value = (value << 6) | (next & 0x3f);
    }
    if ((extra == 2 && value < 0x800) ||
        (extra == 3 && value < 0x10000) ||
        (value >= 0xd800 && value <= 0xdfff) || value > 0x10ffff) {
        diag_error_at(loc, "invalid UTF-8 in character string literal");
        return 0xfffd;
    }
    return value;
}

static void string_data_promote_wide(uint32_t **data, int *len, SourceLoc loc)
{
    uint32_t *wide = arena_alloc((size_t)(*len ? *len : 1) * sizeof(*wide));
    int in = 0;
    int out = 0;

    while (in < *len)
        wide[out++] = decode_utf8_unit(*data, *len, &in, loc);
    *data = wide;
    *len = out;
}

static void string_promote_wide(Node *literal)
{
    string_data_promote_wide(&literal->string_data, &literal->string_len,
                             literal->loc);
    literal->string_wide = 1;
}

static void string_token_promote_wide(StringToken *token, SourceLoc loc)
{
    string_data_promote_wide(&token->data, &token->len, loc);
    token->wide = 1;
}

Node *node_string_append(Node *literal, StringToken *token)
{
    int len;
    uint32_t *data;

    if (literal->string_len >= INT_MAX ||
        token->len > INT_MAX - 1 - literal->string_len) {
        diag_error_at(literal->loc, "character string literal is too long");
        return literal;
    }
    if (literal->string_wide != token->wide) {
        if (!literal->string_wide)
            string_promote_wide(literal);
        if (!token->wide)
            string_token_promote_wide(token, literal->loc);
    }
    len = literal->string_len + token->len;
    data = arena_alloc((size_t)(len > 0 ? len : 1) * sizeof(*data));
    if (literal->string_len > 0)
        memcpy(data, literal->string_data,
               (size_t)literal->string_len * sizeof(*data));
    if (token->len > 0)
        memcpy(data + literal->string_len, token->data,
               (size_t)token->len * sizeof(*data));
    literal->string_data = data;
    literal->string_len = len;
    return literal;
}

Node *string_literals(void)
{
    return literal_head;
}

Node *node_var(char *name, SourceLoc loc)
{
    Node *n = new_node(ND_VAR, loc);
    n->name = name;
    return n;
}

Node *node_binop(BinOp op, Node *l, Node *r, SourceLoc loc)
{
    Node *n = new_node(ND_BINOP, loc);
    n->op = op;
    n->lhs = l;
    n->rhs = r;
    return n;
}

Node *node_pos(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_POS, loc);
    n->operand = o;
    return n;
}

Node *node_neg(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_NEG, loc);
    n->operand = o;
    return n;
}

Node *node_bitnot(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_BITNOT, loc);
    n->operand = o;
    return n;
}

Node *node_preinc(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_PREINC, loc);
    n->operand = o;
    return n;
}

Node *node_predec(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_PREDEC, loc);
    n->operand = o;
    return n;
}

Node *node_postinc(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_POSTINC, loc);
    n->operand = o;
    return n;
}

Node *node_postdec(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_POSTDEC, loc);
    n->operand = o;
    return n;
}

Node *node_not(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_NOT, loc);
    n->operand = o;
    return n;
}

Node *node_logand(Node *l, Node *r, SourceLoc loc)
{
    Node *n = new_node(ND_LOGAND, loc);
    n->lhs = l;
    n->rhs = r;
    return n;
}

Node *node_logor(Node *l, Node *r, SourceLoc loc)
{
    Node *n = new_node(ND_LOGOR, loc);
    n->lhs = l;
    n->rhs = r;
    return n;
}

Node *node_cond(Node *cond, Node *then_expr, Node *else_expr, SourceLoc loc)
{
    Node *n = new_node(ND_COND, loc);
    n->cond = cond;
    n->then_expr = then_expr;
    n->else_expr = else_expr;
    return n;
}

Node *node_addr(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_ADDR, loc);
    n->operand = o;
    return n;
}

Node *node_deref(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_DEREF, loc);
    n->operand = o;
    return n;
}

Node *node_member(Node *base, char *name, SourceLoc loc)
{
    Node *n = new_node(ND_MEMBER, loc);
    n->lhs = base;
    n->name = name;
    return n;
}

Node *node_cast(Type *ty, Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_CAST, loc);
    n->cast_ty = ty;
    n->operand = o;
    return n;
}

Node *node_sizeof_expr(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_SIZEOF, loc);
    n->operand = o;
    return n;
}

Node *node_sizeof_type(Type *ty, SourceLoc loc)
{
    Node *n = new_node(ND_SIZEOF, loc);
    n->cast_ty = ty;
    return n;
}

Node *node_assign(Node *l, Node *r, SourceLoc loc)
{
    Node *n = new_node(ND_ASSIGN, loc);
    n->lhs = l;
    n->rhs = r;
    return n;
}

Node *node_compound_assign(BinOp op, Node *l, Node *r, SourceLoc loc)
{
    Node *n = node_assign(l, r, loc);
    n->is_compound_assign = 1;
    n->op = op;
    return n;
}

Node *node_return(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_RETURN, loc);
    n->operand = o;
    return n;
}

Node *node_expr_stmt(Node *o, SourceLoc loc)
{
    Node *n = new_node(ND_EXPR_STMT, loc);
    n->operand = o;
    return n;
}

Node *node_decl(char *name, Type *spec_ty, Declarator *decl,
                StorageClass storage, Node *init, SourceLoc loc)
{
    Node *n = new_node(ND_DECL, loc);
    n->name = name;
    n->decl_spec = spec_ty;
    n->decl = decl;
    n->decl_storage = storage;
    n->init = init;
    return n;
}

Node *node_init_list(Node *items, SourceLoc loc)
{
    Node *n = new_node(ND_INIT_LIST, loc);
    n->body = items;
    return n;
}

Node *init_list_append(Node *head, Node *item)
{
    item->next = NULL;
    if (!head)
        return item;
    Node *t = head;
    while (t->next)
        t = t->next;
    t->next = item;
    return head;
}

Node *node_call(Node *callee, NodeList *args, SourceLoc loc)
{
    Node *n = new_node(ND_CALL, loc);
    n->callee = callee;
    n->name = (callee && callee->kind == ND_VAR) ? callee->name : NULL;
    n->call_direct = 0;
    n->args = stmt_list_head(args);
    n->nargs = 0;
    for (Node *a = n->args; a; a = a->next)
        n->nargs++;
    return n;
}

static Node *node_builtin_call(const char *name, Node *args, SourceLoc loc)
{
    Node *n = new_node(ND_CALL, loc);
    n->name = (char *)name;
    n->args = args;
    n->nargs = 1;
    return n;
}

Node *node_builtin_va_start(Node *ap, Node *last, SourceLoc loc)
{
    Node *n = node_builtin_call("__builtin_va_start", ap, loc);
    ap->next = last;
    n->nargs = 2;
    return n;
}

Node *node_builtin_va_arg(Node *ap, Type *ty, SourceLoc loc)
{
    Node *n = node_builtin_call("__builtin_va_arg", ap, loc);
    n->cast_ty = ty;
    return n;
}

Node *node_builtin_va_end(Node *ap, SourceLoc loc)
{
    return node_builtin_call("__builtin_va_end", ap, loc);
}

Node *node_builtin_setjmp(Node *env, SourceLoc loc)
{
    return node_builtin_call("__builtin_setjmp", env, loc);
}

Node *node_builtin_longjmp(Node *env, Node *value, SourceLoc loc)
{
    Node *n = node_builtin_call("__builtin_longjmp", env, loc);
    env->next = value;
    n->nargs = 2;
    return n;
}

static Type *offsetof_designator_type(Type *ty, Node *designator,
                                      long *offset, SourceLoc loc)
{
    Member *member;

    if (designator->kind == ND_MEMBER) {
        ty = offsetof_designator_type(ty, designator->lhs, offset, loc);
        if (!ty)
            return NULL;
    } else if (designator->kind != ND_VAR) {
        diag_error_at(loc, "invalid member designator in __builtin_offsetof");
        return NULL;
    }

    member = type_struct_member(ty, designator->name, NULL);
    if (!member) {
        diag_error_at(loc, "no member named '%s' in __builtin_offsetof",
                      designator->name);
        return NULL;
    }
    if (member->is_bitfield) {
        diag_error_at(loc, "cannot apply __builtin_offsetof to bit-field '%s'",
                      designator->name);
        return NULL;
    }
    *offset += member->offset;
    return member->ty;
}

Node *node_builtin_offsetof(Type *ty, Node *designator, SourceLoc loc)
{
    Node *n;
    long offset = 0;

    if (!type_struct_is_complete(ty))
        diag_error_at(loc, "__builtin_offsetof requires a complete struct or union type");
    else
        (void)offsetof_designator_type(ty, designator, &offset, loc);

    n = node_num(offset, loc);
    n->has_long_suffix = 1;
    n->has_unsigned_suffix = 1;
    return n;
}

Node *node_builtin_huge_val(SourceLoc loc)
{
    Node *n = node_num(0, loc);

    n->float_val = (long double)INFINITY;
    n->is_floating_literal = 1;
    return n;
}

Node *node_if(Node *cond, Node *then_body, Node *else_body, SourceLoc loc)
{
    Node *n = new_node(ND_IF, loc);
    n->cond = cond;
    n->then_body = then_body;
    n->else_body = else_body;
    return n;
}

Node *node_while(Node *cond, Node *body, SourceLoc loc)
{
    Node *n = new_node(ND_WHILE, loc);
    n->cond = cond;
    n->then_body = body;
    return n;
}

Node *node_do_while(Node *body, Node *cond, SourceLoc loc)
{
    Node *n = new_node(ND_DO_WHILE, loc);
    n->cond = cond;
    n->then_body = body;
    return n;
}

Node *node_for(Node *init, Node *cond, Node *step, Node *body, SourceLoc loc)
{
    Node *n = new_node(ND_FOR, loc);
    n->init = init;
    n->cond = cond;
    n->step = step;
    n->then_body = body;
    return n;
}

Node *node_switch(Node *cond, Node *body, SourceLoc loc)
{
    Node *n = new_node(ND_SWITCH, loc);
    n->cond = cond;
    n->then_body = body;
    return n;
}

Node *node_case(Node *expr, Node *stmt, SourceLoc loc)
{
    Node *n = new_node(ND_CASE, loc);
    n->operand = expr;
    n->then_body = stmt;
    return n;
}

Node *node_default(Node *stmt, SourceLoc loc)
{
    Node *n = new_node(ND_DEFAULT, loc);
    n->then_body = stmt;
    return n;
}

Node *node_label(char *name, Node *stmt, SourceLoc loc)
{
    Node *n = new_node(ND_LABEL, loc);
    n->name = name;
    n->then_body = stmt;
    n->label = -1;
    return n;
}

Node *node_goto(char *name, SourceLoc loc)
{
    Node *n = new_node(ND_GOTO, loc);
    n->name = name;
    return n;
}

Node *node_break(SourceLoc loc)
{
    return new_node(ND_BREAK, loc);
}

Node *node_continue(SourceLoc loc)
{
    return new_node(ND_CONTINUE, loc);
}

Node *node_block(Node *body, SourceLoc loc)
{
    Node *n = new_node(ND_BLOCK, loc);
    n->body = body;
    return n;
}

TypedefDecl *g_typedef_decls;

TypedefDecl *typedef_decl_new(Type *spec, Declarator *decl, SourceLoc loc)
{
    TypedefDecl *td = arena_alloc_zeroed(sizeof(*td));
    td->spec = spec;
    td->decl = decl;
    td->loc = loc;
    return td;
}

Node *node_typedef(Type *spec, Declarator *decl, SourceLoc loc)
{
    Node *n = new_node(ND_TYPEDEF, loc);
    n->decl_spec = spec;
    n->decl = decl;
    return n;
}

TypedefDecl *typedef_decl_append(TypedefDecl *list, Type *spec, Declarator *decl,
                                 SourceLoc loc)
{
    TypedefDecl *td = arena_alloc_zeroed(sizeof(*td));
    td->spec = spec;
    td->decl = decl;
    td->loc = loc;
    td->next = NULL;

    if (!list)
        return td;

    TypedefDecl *tail = list;
    while (tail->next)
        tail = tail->next;
    tail->next = td;
    return list;
}

StructField *struct_field_append(StructField *list, Type *spec, Declarator *decl,
                                 SourceLoc loc)
{
    StructField *f = arena_alloc_zeroed(sizeof(*f));
    f->spec = spec;
    f->decl = decl;
    f->loc = loc;

    if (!list)
        return f;

    StructField *tail = list;
    while (tail->next)
        tail = tail->next;
    tail->next = f;
    return list;
}

StructField *struct_field_append_bit(StructField *list, Type *spec, Declarator *decl,
                                     Node *bit_width, SourceLoc loc)
{
    if (!decl)
        decl = declarator_empty();
    decl = declarator_bitfield(decl, bit_width);
    return struct_field_append(list, spec, decl, loc);
}

Enumerator *enumerator_new(char *name, Node *value, SourceLoc loc)
{
    Enumerator *e = arena_alloc_zeroed(sizeof(*e));
    e->name = name;
    e->value = value;
    e->loc = loc;
    e->next = NULL;
    return e;
}

Enumerator *enumerator_append(Enumerator *list, Enumerator *e)
{
    Enumerator *tail;

    if (!list)
        return e;
    tail = list;
    while (tail->next)
        tail = tail->next;
    tail->next = e;
    return list;
}

static void member_append(Member **members, int *nmembers, Member *src)
{
    Member *m;
    int n = *nmembers;

    m = arena_alloc((size_t)(n + 1) * sizeof(*m));
    if (n > 0)
        memcpy(m, *members, (size_t)n * sizeof(*m));
    m[n] = *src;
    *members = m;
    (*nmembers)++;
}

Member *struct_fields_to_members(StructField *fields, int *out_n, SourceLoc loc)
{
    Member *members = NULL;
    int nmembers = 0;
    StructField *f;

    for (f = fields; f; f = f->next) {
        char *name = f->decl ? declarator_name(f->decl) : NULL;
        Type *mty;
        Member m = {0};

        if (!f->decl || !f->decl->bit_width_expr) {
            if (!name) {
                diag_error_at(f->loc, "struct member requires a name");
                continue;
            }
            mty = type_apply_declarator(f->spec, f->decl, f->loc);
        } else if (f->decl) {
            mty = type_apply_declarator(f->spec, f->decl, f->loc);
        } else {
            mty = f->spec ? f->spec : type_int();
        }
        m.name = name;
        m.ty = mty;
        m.is_bitfield = f->decl && f->decl->bit_width_expr != NULL;
        member_append(&members, &nmembers, &m);
    }

    if (out_n)
        *out_n = nmembers;
    (void)loc;
    return members;
}

struct NodeList {
    Node *head;
    Node *tail;
};

NodeList *stmt_list_new(void)
{
    return arena_alloc_zeroed(sizeof(NodeList));
}

NodeList *stmt_list_append(NodeList *list, Node *s)
{
    Node *tail;

    if (!list)
        list = stmt_list_new();
    if (!s)
        return list;
    if (!list->head)
        list->head = s;
    else
        list->tail->next = s;
    tail = s;
    while (tail->next)
        tail = tail->next;
    list->tail = tail;
    return list;
}

Node *stmt_list_head(NodeList *list)
{
    return list ? list->head : NULL;
}

Param *param_append(Param *list, Type *ty, char *name)
{
    Param *p = arena_alloc_zeroed(sizeof(Param));
    p->name = name;
    p->ty = ty;
    if (!list)
        return p;
    Param *tail = list;
    while (tail->next)
        tail = tail->next;
    tail->next = p;
    return list;
}

Param *param_append_decl(Param *list, Type *spec_ty, Declarator *decl,
                         char *name)
{
    Param *p = arena_alloc_zeroed(sizeof(Param));
    p->name = name;
    p->decl_spec = spec_ty;
    p->decl = decl;
    if (!list)
        return p;
    Param *tail = list;
    while (tail->next)
        tail = tail->next;
    tail->next = p;
    return list;
}

ParamClause *param_clause(Param *head, int prototyped, int variadic)
{
    ParamClause *pc = arena_alloc_zeroed(sizeof(ParamClause));
    pc->head = head;
    pc->prototyped = prototyped;
    pc->variadic = variadic;
    for (Param *p = head; p; p = p->next)
        pc->count++;
    return pc;
}

Function *func_new(char *name, ParamClause *pc, Type *ret_ty,
                   StorageClass storage, int is_definition, Node *body,
                   SourceLoc loc)
{
    Function *f = arena_alloc_zeroed(sizeof(Function));
    f->name = name;
    f->loc = loc;
    f->storage = storage;
    f->params = pc->head;
    f->nparams = pc->count;
    f->prototyped = pc->prototyped;
    f->variadic = pc->variadic;
    f->ret_ty = ret_ty;
    f->is_definition = is_definition;
    f->body = body;
    f->ty = type_func(ret_ty, NULL, 0, pc->prototyped, pc->variadic);
    return f;
}

Function *func_new_decl(Type *spec, Declarator *decl, StorageClass storage,
                        int is_definition, Node *body, SourceLoc loc)
{
    ParamClause *pc = NULL;
    Type *ty = type_apply_declarator(spec, decl, loc);

    for (Declarator *d = decl; d; d = d->inner) {
        if (d->kind == DECL_FUNC)
            pc = d->func_params;
    }
    if (!ty || ty->kind != TY_FUNC) {
        diag_error_at(loc, "top-level declarator '%s' does not declare a function",
                      declarator_name(decl));
        if (!pc)
            pc = param_clause(NULL, 0, 0);
        return func_new(declarator_name(decl), pc, type_int(), storage,
                        is_definition, body, loc);
    }
    if (!pc)
        pc = param_clause(NULL, ty->prototyped, ty->variadic);
    return func_new(declarator_name(decl), pc, ty->ret, storage,
                    is_definition, body, loc);
}

Function *func_rebuild_type(Function *fn)
{
    Type **ptypes = NULL;
    int n = fn->nparams;

    if (n > 0) {
        ptypes = arena_alloc(sizeof(Type *) * (size_t)n);
        int i = 0;
        for (Param *p = fn->params; p; p = p->next, i++)
            ptypes[i] = type_decay(p->ty);
    }
    fn->ty = type_func(fn->ret_ty, ptypes, n, fn->prototyped, fn->variadic);
    return fn;
}

Function *func_append(Function *list, Function *f)
{
    if (!list)
        return f;
    Function *tail = list;
    while (tail->next)
        tail = tail->next;
    tail->next = f;
    return list;
}

ExternalDecl *external_function(Function *fn)
{
    ExternalDecl *external = arena_alloc_zeroed(sizeof(*external));
    external->kind = EXT_FUNCTION;
    external->function = fn;
    return external;
}

ExternalDecl *external_declaration(Type *spec, Declarator *decl,
                                   StorageClass storage, Node *init,
                                   SourceLoc loc)
{
    Type *ty = type_apply_declarator(spec, decl, loc);

    if (ty && ty->kind == TY_FUNC) {
        if (init)
            diag_error_at(loc, "function '%s' is initialized like an object",
                          declarator_name(decl));
        return external_function(func_new_decl(spec, decl, storage, 0, NULL,
                                               loc));
    }

    ExternalDecl *external = arena_alloc_zeroed(sizeof(*external));
    GlobalObject *object = arena_alloc_zeroed(sizeof(*object));
    object->name = declarator_name(decl);
    object->loc = loc;
    object->storage = storage;
    object->decl_spec = spec;
    object->decl = decl;
    object->init = init;
    external->kind = EXT_OBJECT;
    external->object = object;
    return external;
}

ExternalDecl *external_append(ExternalDecl *list, ExternalDecl *external)
{
    if (!list)
        return external;

    ExternalDecl *tail = list;
    Function *last_function = NULL;
    while (tail->next) {
        if (tail->kind == EXT_FUNCTION)
            last_function = tail->function;
        tail = tail->next;
    }
    if (tail->kind == EXT_FUNCTION)
        last_function = tail->function;
    tail->next = external;
    if (last_function && external->kind == EXT_FUNCTION)
        last_function->next = external->function;
    return list;
}

Function *external_functions(ExternalDecl *list)
{
    for (; list; list = list->next)
        if (list->kind == EXT_FUNCTION)
            return list->function;
    return NULL;
}

Declarator *declarator_empty(void)
{
    Declarator *d = arena_alloc_zeroed(sizeof(Declarator));
    d->kind = DECL_IDENT;
    return d;
}

Declarator *declarator_ident(char *name)
{
    Declarator *d = declarator_empty();
    d->name = name;
    return d;
}

Declarator *declarator_bitfield(Declarator *d, Node *width)
{
    d->bit_width_expr = width;
    return d;
}

Declarator *declarator_ptr(Declarator *d, unsigned qualifiers)
{
    Declarator *wrap = declarator_empty();
    wrap->kind = DECL_PTR;
    wrap->inner = d;
    wrap->qualifiers = qualifiers;
    return wrap;
}

Declarator *declarator_suffix(Declarator *d, Node *dim)
{
    Declarator *wrap = declarator_empty();
    wrap->kind = DECL_ARRAY;
    wrap->inner = d;
    wrap->array_dim = dim;
    return wrap;
}

Declarator *declarator_add_dim(Declarator *d, Node *dim, int after_paren)
{
    (void)after_paren;
    return declarator_suffix(d, dim);
}

Declarator *declarator_paren_group(Declarator *d)
{
    return d;
}

Declarator *declarator_paren_outer(Declarator *d, Node *dim)
{
    return declarator_suffix(d, dim);
}

Declarator *declarator_func(Declarator *d, ParamClause *pc)
{
    Declarator *wrap;

    if (!d)
        d = declarator_empty();
    wrap = declarator_empty();
    wrap->kind = DECL_FUNC;
    wrap->inner = d;
    wrap->func_params = pc;
    return wrap;
}

int declarator_was_paren(const Declarator *d)
{
    (void)d;
    return 0;
}

char *declarator_name(const Declarator *d)
{
    for (const Declarator *cur = d; cur; cur = cur->inner) {
        if (cur->name)
            return cur->name;
    }
    return NULL;
}

ParamClause *declarator_function_params(const Declarator *d)
{
    ParamClause *pc = NULL;

    for (const Declarator *cur = d; cur; cur = cur->inner) {
        if (cur->kind == DECL_FUNC)
            pc = cur->func_params;
    }
    return pc;
}

static long ast_sizeof_type(Type *ty, SourceLoc loc)
{
    if (!ty || type_is_void(ty)) {
        diag_error_at(loc, "invalid application of sizeof to void type");
        return 1;
    }
    if (ty->kind == TY_FUNC) {
        diag_error_at(loc, "invalid application of sizeof to function type");
        return 1;
    }
    if (!type_is_complete(ty)) {
        diag_error_at(loc, "invalid application of sizeof to incomplete type");
        return 1;
    }
    return type_size(ty);
}

static int parse_abstract_sizeof(Node *expr, long *out, void *ctx)
{
    SourceLoc *loc = ctx;

    if (!expr || expr->operand || !loc)
        return 0;
    *out = ast_sizeof_type(expr->cast_ty, *loc);
    return 1;
}

static int parse_abstract_ice_lookup(const char *name, long *out, Type **out_ty,
                                     void *ctx)
{
    (void)ctx;
    if (!enum_const_lookup(name, out))
        return 0;
    if (out_ty)
        *out_ty = type_int();
    return 1;
}

static int parse_abstract_dim_eval(Node *expr, long *out, SourceLoc loc, void *ctx)
{
    (void)ctx;
    if (!expr) {
        *out = 0;
        return 1;
    }
    if (!int_const_eval(expr, parse_abstract_ice_lookup, parse_abstract_sizeof,
                      &loc, out, NULL)) {
        diag_error_at(loc, "array size is not an integer constant expression");
        return 0;
    }
    return 1;
}

static int eval_decl_dim(Node *expr, long *out, SourceLoc loc, DeclDimEvalFn eval,
                         void *ctx)
{
    if (!expr) {
        *out = 0;
        return 1;
    }
    if (eval)
        return eval(expr, out, loc, ctx);
    if (expr->kind == ND_NUM) {
        *out = expr->val;
        return 1;
    }
    diag_error_at(loc, "array size is not an integer constant expression");
    return 0;
}

static Type *make_array_dim(Type *ty, Node *dim, SourceLoc loc,
                            DeclDimEvalFn eval, void *ctx)
{
    int count;
    int esz;
    long bound;

    if (!eval_decl_dim(dim, &bound, loc, eval, ctx))
        return ty;
    if (bound == 0) {
        if (dim) {
            diag_error_at(loc, "array size is zero");
            return ty;
        }
        /* An unsized `[]` parameter decays to a pointer before allocation. */
        return type_array(ty, 0);
    }
    if (bound < 0) {
        diag_error_at(loc, "array size is negative");
        return ty;
    }
    if (bound > INT_MAX) {
        diag_error_at(loc, "array size is too large");
        return ty;
    }
    count = (int)bound;
    esz = type_size(ty);
    if (esz > 0 && count > INT_MAX / esz) {
        diag_error_at(loc, "array size overflows");
        return ty;
    }
    return type_array(ty, count);
}

static Type *type_func_from_params(Type *ret, ParamClause *pc, SourceLoc loc)
{
    Type **ptypes = NULL;
    int n = 0;

    if (pc && pc->head) {
        for (Param *p = pc->head; p; p = p->next)
            n++;
        if (n > 0) {
            ptypes = arena_alloc((size_t)n * sizeof(*ptypes));
            int i = 0;
            for (Param *p = pc->head; p; p = p->next, i++) {
                Type *pty;

                if (p->decl)
                    pty = type_apply_declarator(p->decl_spec, p->decl, loc);
                else if (p->decl_spec)
                    pty = p->decl_spec;
                else
                    pty = p->ty;

                if (type_is_void(pty)) {
                    diag_error_at(loc, "parameter must not have void type");
                } else if (type_is_array(pty) &&
                           !type_is_complete(type_array_elem(pty))) {
                    diag_error_at(loc,
                                  "array parameter has incomplete element type '%s'",
                                  type_name(type_array_elem(pty)));
                }
                for (Param *prev = pc->head; prev != p; prev = prev->next) {
                    if (p->name && prev->name &&
                        strcmp(p->name, prev->name) == 0) {
                        diag_error_at(loc, "redefinition of parameter '%s'",
                                      p->name);
                        break;
                    }
                }
                ptypes[i] = type_unqualified(type_decay(pty));
            }
        }
    }

    if (ret && ret->kind == TY_FUNC)
        diag_error_at(loc, "function cannot return function type");
    else if (ret && ret->kind == TY_ARRAY)
        diag_error_at(loc, "function cannot return array type");
    return type_func(ret, ptypes, n, pc ? pc->prototyped : 0,
                     pc ? pc->variadic : 0);
}

Type *type_apply_declarator_cb(Type *base, Declarator *d, SourceLoc loc,
                               DeclDimEvalFn eval, void *ctx)
{
    if (!d)
        return base;

    switch (d->kind) {
    case DECL_IDENT:
        return base;
    case DECL_PTR:
        return type_apply_declarator_cb(
            type_qualify(type_ptr(base), d->qualifiers),
            d->inner, loc, eval, ctx);
    case DECL_ARRAY:
        if (base && base->kind == TY_FUNC)
            diag_error_at(loc, "array element type must not be a function type");
        return type_apply_declarator_cb(
            make_array_dim(base, d->array_dim, loc, eval, ctx),
            d->inner, loc, eval, ctx);
    case DECL_FUNC:
        return type_apply_declarator_cb(
            type_func_from_params(base, d->func_params, loc),
            d->inner, loc, eval, ctx);
    }
    return base;
}

Type *type_apply_declarator(Type *base, Declarator *d, SourceLoc loc)
{
    return type_apply_declarator_cb(base, d, loc, parse_abstract_dim_eval, NULL);
}
