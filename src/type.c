/* SPDX-License-Identifier: MIT */
#include "type.h"
#include "arena.h"

#include <string.h>
#include <stdio.h>

/* ---- builtin singletons ----
 *
 * char is modeled with its true C size (1). int uses 4 here as the semantic
 * size; codegen may still lower both through 8-byte slots in 0.0.1.3 (a known
 * temporary backend limitation, documented in typesystemlist.txt). Pointers
 * are 8 bytes / 8-aligned on x86-64 SysV. */

static Type ty_void = { TY_VOID, 0, 1, NULL, NULL, NULL, 0, 0 };
static Type ty_char = { TY_CHAR, 1, 1, NULL, NULL, NULL, 0, 0 };
static Type ty_int  = { TY_INT,  4, 4, NULL, NULL, NULL, 0, 0 };

Type *type_void(void) { return &ty_void; }
Type *type_char(void) { return &ty_char; }
Type *type_int(void)  { return &ty_int; }

Type *type_ptr(Type *base)
{
    Type *t = arena_alloc_zeroed(sizeof(Type));
    t->kind = TY_PTR;
    t->size = 8;
    t->align = 8;
    t->base = base;
    return t;
}

Type *type_func(Type *ret, Type **params, int nparams, int prototyped)
{
    Type *t = arena_alloc_zeroed(sizeof(Type));
    t->kind = TY_FUNC;
    t->size = 0;
    t->align = 1;
    t->ret = ret;
    t->params = params;
    t->nparams = nparams;
    t->prototyped = prototyped;
    return t;
}

/* ---- predicates ---- */

int type_is_void(Type *ty)    { return ty && ty->kind == TY_VOID; }

int type_is_char(Type *ty)    { return ty && ty->kind == TY_CHAR; }

int type_is_integer(Type *ty)
{
    return ty && (ty->kind == TY_CHAR || ty->kind == TY_INT);
}

int type_is_pointer(Type *ty) { return ty && ty->kind == TY_PTR; }

int type_is_scalar(Type *ty)
{
    return type_is_integer(ty) || type_is_pointer(ty);
}

int type_is_object(Type *ty)
{
    /* Object types have storage. void and functions do not. */
    return ty && ty->kind != TY_VOID && ty->kind != TY_FUNC;
}

/* ---- relations ---- */

int type_same(Type *a, Type *b)
{
    if (a == b)
        return 1;
    if (!a || !b || a->kind != b->kind)
        return 0;

    switch (a->kind) {
    case TY_VOID:
    case TY_CHAR:
    case TY_INT:
        return 1;
    case TY_PTR:
        return type_same(a->base, b->base);
    case TY_FUNC:
        if (!type_same(a->ret, b->ret))
            return 0;
        if (a->prototyped && b->prototyped) {
            if (a->nparams != b->nparams)
                return 0;
            for (int i = 0; i < a->nparams; i++)
                if (!type_same(a->params[i], b->params[i]))
                    return 0;
        }
        return 1;
    }
    return 0;
}

/* A void pointer (pointer to void). */
static int is_void_ptr(Type *ty)
{
    return type_is_pointer(ty) && type_is_void(ty->base);
}

/* Two types are compatible for pointer comparison / conversion purposes. */
int type_compatible(Type *a, Type *b)
{
    if (type_same(a, b))
        return 1;

    /* void * is compatible with any object pointer (and vice versa). */
    if (type_is_pointer(a) && type_is_pointer(b)) {
        if (is_void_ptr(a) || is_void_ptr(b))
            return 1;
        return type_same(a->base, b->base);
    }
    return 0;
}

int type_assignable(Type *dst, Type *src)
{
    if (!dst || !src)
        return 0;

    /* integer <- integer (char and int interchange in 0.0.1.3). */
    if (type_is_integer(dst) && type_is_integer(src))
        return 1;

    /* pointer <- compatible pointer, including void * conversions. */
    if (type_is_pointer(dst) && type_is_pointer(src))
        return type_compatible(dst, src);

    return 0;
}

/* ---- queries ---- */

int type_size(Type *ty)  { return ty ? ty->size : 0; }
int type_align(Type *ty) { return ty ? ty->align : 1; }

const char *type_name(Type *ty)
{
    if (!ty)
        return "<null>";

    switch (ty->kind) {
    case TY_VOID: return "void";
    case TY_CHAR: return "char";
    case TY_INT:  return "int";
    case TY_PTR: {
        /* "T *" — build recursively into an arena buffer. */
        const char *inner = type_name(ty->base);
        size_t n = strlen(inner) + 3; /* inner + " *" + NUL */
        char *buf = arena_alloc(n);
        snprintf(buf, n, "%s *", inner);
        return buf;
    }
    case TY_FUNC:
        return "function";
    }
    return "<type>";
}

const char *type_func_sig(Type *ty)
{
    char *buf;
    size_t n;
    int pos;

    if (!ty || ty->kind != TY_FUNC)
        return type_name(ty);

    if (!ty->prototyped) {
        n = strlen(type_name(ty->ret)) + 3;
        buf = arena_alloc(n);
        snprintf(buf, n, "%s()", type_name(ty->ret));
        return buf;
    }

    if (ty->nparams == 0) {
        n = strlen(type_name(ty->ret)) + 8;
        buf = arena_alloc(n);
        snprintf(buf, n, "%s(void)", type_name(ty->ret));
        return buf;
    }

    n = strlen(type_name(ty->ret)) + 3;
    for (int i = 0; i < ty->nparams; i++)
        n += strlen(type_name(ty->params[i])) + 2;

    buf = arena_alloc(n);
    pos = snprintf(buf, n, "%s(", type_name(ty->ret));
    for (int i = 0; i < ty->nparams; i++) {
        if (i > 0)
            pos += snprintf(buf + pos, n - (size_t)pos, ", ");
        pos += snprintf(buf + pos, n - (size_t)pos, "%s",
                        type_name(ty->params[i]));
    }
    snprintf(buf + pos, n - (size_t)pos, ")");
    return buf;
}
