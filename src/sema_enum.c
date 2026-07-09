/* SPDX-License-Identifier: MIT */
#include "sema_enum.h"
#include "diag.h"
#include "arena.h"
#include "intconst.h"

#include <string.h>

#define MAX_ENUM_TAGS 256
#define MAX_ENUM_CONSTS 4096

typedef struct {
    char *tag;
    Type *ty;
    SourceLoc loc;
} EnumTagEntry;

typedef struct {
    char *name;
    long value;
    SourceLoc loc;
} EnumConstEntry;

static EnumTagEntry tags[MAX_ENUM_TAGS];
static int ntags;

static EnumConstEntry consts[MAX_ENUM_CONSTS];
static int nconsts;

void enum_reset(void)
{
    ntags = 0;
    nconsts = 0;
}

static Type *enum_tag_lookup(const char *tag)
{
    for (int i = 0; i < ntags; i++) {
        if (strcmp(tags[i].tag, tag) == 0)
            return tags[i].ty;
    }
    return NULL;
}

int enum_const_lookup(const char *name, long *out_value)
{
    for (int i = 0; i < nconsts; i++) {
        if (strcmp(consts[i].name, name) == 0) {
            if (out_value)
                *out_value = consts[i].value;
            return 1;
        }
    }
    return 0;
}

/* Constant folding for enumerator values. Unlike the struct bit-field ICE
 * evaluator, this must also resolve references to earlier enumerators. */
static int enum_ice_eval(Node *n, long *out)
{
    if (!n)
        return 0;

    switch (n->kind) {
    case ND_NUM:
        *out = n->val;
        return 1;
    case ND_VAR:
        return enum_const_lookup(n->name, out);
    case ND_NEG:
        if (!enum_ice_eval(n->operand, out))
            return 0;
        return int_const_neg(*out, out);
    case ND_BINOP:
        if (n->op == OP_COMMA)
            return enum_ice_eval(n->rhs, out);
        {
            long l, r;
            if (!enum_ice_eval(n->lhs, &l) || !enum_ice_eval(n->rhs, &r))
                return 0;
            return int_const_binop(n->op, l, r, out);
        }
    default:
        return 0;
    }
}

static void enum_const_register(char *name, long value, SourceLoc loc)
{
    if (enum_const_lookup(name, NULL)) {
        diag_error_at(loc, "redefinition of enumerator '%s'", name);
        return;
    }
    if (nconsts >= MAX_ENUM_CONSTS) {
        diag_error_at(loc, "too many enumerator constants");
        return;
    }
    consts[nconsts].name = name;
    consts[nconsts].value = value;
    consts[nconsts].loc = loc;
    nconsts++;
}

Type *enum_tag_forward(char *tag, SourceLoc loc)
{
    Type *ty = enum_tag_lookup(tag);

    if (ty)
        return ty;

    ty = type_enum(tag);
    if (ntags >= MAX_ENUM_TAGS) {
        diag_error_at(loc, "too many enum tags");
        return ty;
    }
    tags[ntags].tag = tag;
    tags[ntags].ty = ty;
    tags[ntags].loc = loc;
    ntags++;
    return ty;
}

Type *enum_tag_define(char *tag, Enumerator *list, SourceLoc loc)
{
    Type *ty;
    Enumerator *e;
    long next = 0;

    if (tag) {
        ty = enum_tag_lookup(tag);
        if (ty && ty->is_complete) {
            diag_error_at(loc, "redefinition of enum '%s'", tag);
            return ty;
        }
        if (!ty)
            ty = enum_tag_forward(tag, loc);
    } else {
        ty = type_enum(NULL);
    }

    for (e = list; e; e = e->next) {
        long value = next;

        if (e->value) {
            if (!enum_ice_eval(e->value, &value)) {
                diag_error_at(e->loc,
                              "enumerator value is not an integer constant "
                              "expression");
                value = next;
            }
        }
        enum_const_register(e->name, value, e->loc);
        next = value + 1;
    }

    ty->is_complete = 1;
    return ty;
}
