/* SPDX-License-Identifier: MIT */
#include "type.h"
#include "arena.h"

#include <string.h>
#include <stdio.h>

/* char 1 byte; short 2 bytes; int 4 bytes; long 8 bytes on x86-64 LP64 SysV. */

static Type ty_void  = { TY_VOID, 0, 0, 0, 1, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_char  = { TY_INT, IW_CHAR,  IS_SIGNED, 1, 1, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_short = { TY_INT, IW_SHORT, IS_SIGNED, 2, 2, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_int   = { TY_INT, IW_INT,   IS_SIGNED, 4, 4, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_long  = { TY_INT, IW_LONG,  IS_SIGNED, 8, 8, NULL, 0, NULL, NULL, 0, 0 };

Type *type_void(void)  { return &ty_void; }
Type *type_char(void)  { return &ty_char; }
Type *type_short(void) { return &ty_short; }
Type *type_int(void)   { return &ty_int; }
Type *type_long(void)  { return &ty_long; }

Type *type_ptr(Type *base)
{
    Type *t = arena_alloc_zeroed(sizeof(Type));
    t->kind = TY_PTR;
    t->size = 8;
    t->align = 8;
    t->base = base;
    return t;
}

Type *type_array(Type *elem, int count)
{
    Type *t = arena_alloc_zeroed(sizeof(Type));
    t->kind = TY_ARRAY;
    t->base = elem;
    t->count = count;
    t->size = type_size(elem) * count;
    t->align = type_align(elem);
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

int type_is_char(Type *ty)
{
    return ty && ty->kind == TY_INT && ty->width == IW_CHAR;
}

int type_is_long(Type *ty)
{
    return ty && ty->kind == TY_INT && ty->width == IW_LONG;
}

int type_is_short(Type *ty)
{
    return ty && ty->kind == TY_INT && ty->width == IW_SHORT;
}

int type_is_integer(Type *ty) { return ty && ty->kind == TY_INT; }

int type_is_signed(Type *ty)
{
    return ty && ty->kind == TY_INT && ty->sign == IS_SIGNED;
}

int type_is_pointer(Type *ty) { return ty && ty->kind == TY_PTR; }

int type_is_array(Type *ty)   { return ty && ty->kind == TY_ARRAY; }

int type_is_scalar(Type *ty)
{
    return type_is_integer(ty) || type_is_pointer(ty);
}

int type_is_object(Type *ty)
{
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
        return 1;
    case TY_INT:
        return a->width == b->width && a->sign == b->sign;
    case TY_PTR:
        return type_same(a->base, b->base);
    case TY_ARRAY:
        return a->count == b->count &&
               type_same(a->base, b->base);
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

static int is_void_ptr(Type *ty)
{
    return type_is_pointer(ty) && type_is_void(ty->base);
}

int type_compatible(Type *a, Type *b)
{
    if (type_same(a, b))
        return 1;

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

    if (type_is_array(dst) || type_is_array(src))
        return 0;

    if (type_is_integer(dst) && type_is_integer(src))
        return 1;

    if (type_is_pointer(dst) && type_is_pointer(src))
        return type_compatible(dst, src);

    return 0;
}

/* ---- queries ---- */

int type_int_width(Type *ty)
{
    if (!ty || ty->kind != TY_INT)
        return 0;

    switch (ty->width) {
    case IW_CHAR:  return 1;
    case IW_SHORT: return 2;
    case IW_INT:   return 4;
    case IW_LONG:  return 8;
    }
    return 0;
}

int type_int_rank(Type *ty)
{
    if (!ty || ty->kind != TY_INT)
        return 0;

    switch (ty->width) {
    case IW_CHAR:  return 1;
    case IW_SHORT: return 2;
    case IW_INT:   return 3;
    case IW_LONG:  return 4;
    }
    return 0;
}

Type *type_int_promote(Type *ty)
{
    if (!ty || ty->kind != TY_INT)
        return ty;
    if (ty->width == IW_CHAR || ty->width == IW_SHORT)
        return type_int();
    return ty;
}

Type *type_arith_convert(Type *a, Type *b)
{
    a = type_int_promote(a);
    b = type_int_promote(b);
    if (!a || !b || !type_is_integer(a) || !type_is_integer(b))
        return type_int();
    if (a->width == IW_LONG || b->width == IW_LONG)
        return type_long();
    return type_int();
}

int type_size(Type *ty)
{
    if (!ty)
        return 0;
    if (ty->kind == TY_INT)
        return type_int_width(ty);
    return ty->size;
}

int type_align(Type *ty)
{
    if (!ty)
        return 1;
    if (ty->kind == TY_INT)
        return type_int_width(ty);
    return ty->align;
}

Type *type_decay(Type *ty)
{
    if (type_is_array(ty))
        return type_ptr(type_array_elem(ty));
    return ty;
}

Type *type_array_elem(Type *ty)
{
    return type_is_array(ty) ? ty->base : NULL;
}

int type_array_count(Type *ty)
{
    return type_is_array(ty) ? ty->count : 0;
}

Type *type_ptr_elem(Type *ty)
{
    return type_is_pointer(ty) ? ty->base : NULL;
}

const char *type_name(Type *ty)
{
    if (!ty)
        return "<null>";

    switch (ty->kind) {
    case TY_VOID: return "void";
    case TY_INT:
        switch (ty->width) {
        case IW_CHAR:  return "char";
        case IW_SHORT: return "short";
        case IW_INT:   return "int";
        case IW_LONG:  return "long";
        }
        return "integer";
    case TY_PTR: {
        const char *inner = type_name(ty->base);
        size_t n = strlen(inner) + 3;
        char *buf = arena_alloc(n);
        snprintf(buf, n, "%s *", inner);
        return buf;
    }
    case TY_ARRAY: {
        const char *inner = type_name(ty->base);
        size_t n = strlen(inner) + 32;
        char *buf = arena_alloc(n);
        snprintf(buf, n, "%s[%d]", inner, ty->count);
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
