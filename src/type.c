/* SPDX-License-Identifier: MIT */
#include "type.h"
#include "arena.h"

#include <limits.h>
#include <string.h>
#include <stdio.h>

/* char 1 byte; short 2 bytes; int 4 bytes; long 8 bytes on x86-64 LP64 SysV. */

static Type ty_void          = { TY_VOID, 0, 0,          0, 1, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_char          = { TY_INT, IW_CHAR,  IS_SIGNED,   1, 1, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_schar         = { TY_INT, IW_CHAR,  IS_SIGNED,   1, 1, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_uchar         = { TY_INT, IW_CHAR,  IS_UNSIGNED, 1, 1, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_short         = { TY_INT, IW_SHORT, IS_SIGNED,   2, 2, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_ushort        = { TY_INT, IW_SHORT, IS_UNSIGNED, 2, 2, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_int           = { TY_INT, IW_INT,   IS_SIGNED,   4, 4, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_uint          = { TY_INT, IW_INT,   IS_UNSIGNED, 4, 4, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_long          = { TY_INT, IW_LONG,  IS_SIGNED,   8, 8, NULL, 0, NULL, NULL, 0, 0 };
static Type ty_ulong         = { TY_INT, IW_LONG,  IS_UNSIGNED, 8, 8, NULL, 0, NULL, NULL, 0, 0 };

Type *type_void(void)           { return &ty_void; }
Type *type_char(void)           { return &ty_char; }
Type *type_short(void)          { return &ty_short; }
Type *type_int(void)            { return &ty_int; }
Type *type_long(void)           { return &ty_long; }
Type *type_signed_char(void)    { return &ty_schar; }
Type *type_unsigned_char(void)  { return &ty_uchar; }
Type *type_unsigned_short(void) { return &ty_ushort; }
Type *type_unsigned_int(void)   { return &ty_uint; }
Type *type_unsigned_long(void)  { return &ty_ulong; }

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

Type *type_typedef_ref(char *name)
{
    Type *t = arena_alloc_zeroed(sizeof(Type));
    t->kind = TY_TYPEDEF_REF;
    t->ref_name = name;
    return t;
}

/* An enumerated type. C89 3.5.2.2: an enum has type `int`; xcc keeps a distinct
 * kind for diagnostics but reports int width/sign/rank everywhere. */
Type *type_enum(char *tag)
{
    Type *t = arena_alloc_zeroed(sizeof(Type));
    t->kind = TY_ENUM;
    t->tag = tag;
    t->is_complete = 0;
    t->width = IW_INT;
    t->sign = IS_SIGNED;
    t->size = 4;
    t->align = 4;
    return t;
}

/* ---- predicates ---- */

int type_is_void(Type *ty)    { return ty && ty->kind == TY_VOID; }

int type_is_plain_char(Type *ty)
{
    return ty == &ty_char;
}

int type_is_signed_char(Type *ty)
{
    return ty == &ty_schar;
}

int type_is_unsigned_char(Type *ty)
{
    return ty == &ty_uchar;
}

int type_is_char(Type *ty)
{
    return type_is_plain_char(ty) || type_is_signed_char(ty) ||
           type_is_unsigned_char(ty);
}

int type_is_long(Type *ty)
{
    return ty && ty->kind == TY_INT && ty->width == IW_LONG;
}

int type_is_short(Type *ty)
{
    return ty && ty->kind == TY_INT && ty->width == IW_SHORT;
}

int type_is_integer(Type *ty)
{
    return ty && (ty->kind == TY_INT || ty->kind == TY_ENUM);
}

int type_is_signed(Type *ty)
{
    if (!ty)
        return 0;
    if (ty->kind == TY_ENUM)
        return 1;
    return ty->kind == TY_INT && ty->sign == IS_SIGNED;
}

int type_is_unsigned(Type *ty)
{
    return ty && ty->kind == TY_INT && ty->sign == IS_UNSIGNED;
}

int type_is_pointer(Type *ty) { return ty && ty->kind == TY_PTR; }

int type_is_typedef_ref(Type *ty)
{
    return ty && ty->kind == TY_TYPEDEF_REF;
}

const char *type_typedef_ref_name(Type *ty)
{
    return type_is_typedef_ref(ty) ? ty->ref_name : NULL;
}

int type_is_array(Type *ty)   { return ty && ty->kind == TY_ARRAY; }

int type_is_struct(Type *ty)  { return ty && ty->kind == TY_STRUCT; }

int type_is_union(Type *ty)   { return ty && ty->kind == TY_UNION; }

int type_is_record(Type *ty)
{
    return ty && (ty->kind == TY_STRUCT || ty->kind == TY_UNION);
}

int type_is_enum(Type *ty)    { return ty && ty->kind == TY_ENUM; }

int type_struct_is_complete(Type *ty)
{
    return type_is_record(ty) && ty->is_complete;
}

const char *type_struct_tag(Type *ty)
{
    return type_is_record(ty) ? ty->tag : NULL;
}

static int align_up(int off, int align)
{
    if (align <= 1)
        return off;
    return (off + align - 1) & ~(align - 1);
}

void type_struct_layout(Type *ty)
{
    int off = 0;
    int max_align = 1;
    int unit_off = 0;
    int unit_bits = 0;
    int unit_size = 0;

    if (!type_is_record(ty))
        return;

    /* Union: every member (bit-field or not) starts at offset 0; the object is
     * as large as its widest member, aligned to the strictest member. */
    if (type_is_union(ty)) {
        int sz = 0;

        for (int i = 0; i < ty->nmembers; i++) {
            Member *m = &ty->members[i];
            int ma = type_align(m->ty);
            int ms = m->is_bitfield ? (int)sizeof(int) : type_size(m->ty);

            m->offset = 0;
            m->bit_offset = 0;
            if (ma > max_align)
                max_align = ma;
            if (ms > sz)
                sz = ms;
        }
        ty->align = max_align > 0 ? max_align : 1;
        ty->size = sz > 0 ? align_up(sz, ty->align) : 0;
        return;
    }

    for (int i = 0; i < ty->nmembers; i++) {
        Member *m = &ty->members[i];

        if (m->is_bitfield) {
            int width = m->bit_width;
            int bsize = (int)sizeof(int);

            if (width == 0) {
                if (unit_bits > 0) {
                    off = unit_off + unit_size;
                    unit_bits = 0;
                    unit_size = 0;
                }
                continue;
            }

            if (unit_size == 0 || unit_bits + width > bsize * 8) {
                if (unit_bits > 0)
                    off = unit_off + unit_size;
                unit_off = align_up(off, bsize);
                off = unit_off;
                unit_bits = 0;
                unit_size = bsize;
            }

            m->offset = unit_off;
            m->bit_offset = unit_bits;
            unit_bits += width;
            if (bsize > max_align)
                max_align = bsize;
        } else {
            Type *mty = m->ty;
            int ma;

            if (unit_bits > 0) {
                off = unit_off + unit_size;
                unit_bits = 0;
                unit_size = 0;
            }
            ma = type_align(mty);
            if (ma > max_align)
                max_align = ma;
            off = align_up(off, ma);
            m->offset = off;
            m->bit_offset = 0;
            off += type_size(mty);
        }
    }

    if (unit_bits > 0)
        off = unit_off + unit_size;

    ty->align = max_align > 0 ? max_align : 1;
    ty->size = off > 0 ? align_up(off, ty->align) : 0;
}

Member *type_struct_member(Type *ty, const char *name, int *out_index)
{
    if (!type_is_record(ty) || !name)
        return NULL;

    for (int i = 0; i < ty->nmembers; i++) {
        if (ty->members[i].name && strcmp(ty->members[i].name, name) == 0) {
            if (out_index)
                *out_index = i;
            return &ty->members[i];
        }
    }
    return NULL;
}

int type_is_scalar(Type *ty)
{
    return type_is_integer(ty) || type_is_pointer(ty);
}

static int type_cast_member_ty_ok(Type *ty)
{
    if (type_is_scalar(ty))
        return 1;
    if (type_is_array(ty)) {
        Type *elem = type_array_elem(ty);
        return elem && type_is_scalar(elem);
    }
    return 0;
}

/* C89 3.3.4: cast target may be void, scalar, or a struct whose members are
 * scalars or arrays of scalars only (B11 / D16). */
int type_cast_target_ok(Type *ty)
{
    if (!ty)
        return 0;
    if (type_is_void(ty) || type_is_scalar(ty))
        return 1;
    if (!type_is_struct(ty) || !type_struct_is_complete(ty))
        return 0;

    for (int i = 0; i < ty->nmembers; i++) {
        Member *m = &ty->members[i];

        if (m->is_bitfield) {
            if (!type_is_integer(m->ty))
                return 0;
            continue;
        }
        if (!type_cast_member_ty_ok(m->ty))
            return 0;
    }
    return 1;
}

int type_is_object(Type *ty)
{
    return ty && ty->kind != TY_VOID && ty->kind != TY_FUNC;
}

int type_is_complete(Type *ty)
{
    if (!ty || type_is_void(ty) || ty->kind == TY_FUNC || type_is_typedef_ref(ty))
        return 0;
    if (type_is_pointer(ty))
        return 1;
    if (type_is_record(ty) || type_is_enum(ty))
        return ty->is_complete;
    if (type_is_array(ty) && type_array_count(ty) == 0)
        return 0;
    if (type_is_array(ty) && !type_is_complete(type_array_elem(ty)))
        return 0;
    return 1;
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
        if (type_is_char(a) || type_is_char(b)) {
            if (!type_is_char(a) || !type_is_char(b))
                return 0;
            return a == b;
        }
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
    case TY_TYPEDEF_REF:
        return a->ref_name && b->ref_name &&
               strcmp(a->ref_name, b->ref_name) == 0;
    case TY_STRUCT:
    case TY_UNION:
    case TY_ENUM:
        return a->tag && b->tag && strcmp(a->tag, b->tag) == 0;
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

    if (type_is_record(dst) && type_is_record(src))
        return type_same(dst, src);

    return 0;
}

/* ---- queries ---- */

int type_int_width(Type *ty)
{
    TypeIntInfo info;
    return type_int_info(ty, &info) ? info.width : 0;
}

static long sign_extend_masked(unsigned long v, int bits)
{
    unsigned long mask;
    unsigned long sign;

    if (bits >= 8 * (int)sizeof(long))
        return (long)v;

    mask = (1UL << bits) - 1;
    sign = 1UL << (bits - 1);
    v &= mask;
    if (v & sign)
        return -(long)((~v & mask) + 1);
    return (long)v;
}

long type_convert_const(long v, Type *ty)
{
    TypeIntInfo info;
    int bits;
    unsigned long uv;

    if (!type_int_info(ty, &info))
        return v;

    bits = info.width * 8;
    uv = (unsigned long)v;

    if (!info.is_signed) {
        if (info.width >= 8)
            return (long)uv;
        return (long)(uv & ((1UL << bits) - 1));
    }

    return sign_extend_masked(uv, bits);
}

int type_int_rank(Type *ty)
{
    TypeIntInfo info;
    return type_int_info(ty, &info) ? info.rank : 0;
}

int type_int_info(Type *ty, TypeIntInfo *out)
{
    if (!ty || (ty->kind != TY_INT && ty->kind != TY_ENUM))
        return 0;

    /* C89 3.5.2.2: an enumeration has type int. */
    if (ty->kind == TY_ENUM) {
        if (out) {
            out->width = 4;
            out->rank = 3;
            out->is_signed = 1;
        }
        return 1;
    }

    if (out)
        out->is_signed = type_is_signed(ty) && !type_is_plain_char(ty);

    switch (ty->width) {
    case IW_CHAR:
        if (out) {
            out->width = 1;
            out->rank = 1;
        }
        return 1;
    case IW_SHORT:
        if (out) {
            out->width = 2;
            out->rank = 2;
        }
        return 1;
    case IW_INT:
        if (out) {
            out->width = 4;
            out->rank = 3;
        }
        return 1;
    case IW_LONG:
        if (out) {
            out->width = 8;
            out->rank = 4;
        }
        return 1;
    }
    return 0;
}

int type_scalar_info(Type *ty, TypeScalarInfo *out)
{
    TypeIntInfo ii;

    if (type_int_info(ty, &ii)) {
        if (out) {
            out->width = ii.width;
            out->is_signed = ii.is_signed;
            out->rank = ii.rank;
            out->is_integer = 1;
            out->is_pointer = 0;
        }
        return 1;
    }

    if (type_is_pointer(ty)) {
        if (out) {
            out->width = 8;
            out->is_signed = 0;
            out->rank = 0;
            out->is_integer = 0;
            out->is_pointer = 1;
        }
        return 1;
    }

    return 0;
}

static Type *type_unsigned_same_width(Type *ty)
{
    switch (ty->width) {
    case IW_CHAR:  return type_unsigned_char();
    case IW_SHORT: return type_unsigned_short();
    case IW_INT:   return type_unsigned_int();
    case IW_LONG:  return type_unsigned_long();
    }
    return type_unsigned_int();
}

/* C89 3.2.1.1 integral promotion: narrow integers promote to int when int can
 * represent every value of the original type; otherwise unsigned int.
 * On LP64 xcc, int holds every char/short value (signed or unsigned). */
Type *type_int_promote(Type *ty)
{
    if (!ty)
        return ty;
    if (ty->kind == TY_ENUM)   /* an enum promotes to int */
        return type_int();
    if (ty->kind != TY_INT)
        return ty;
    if (ty->width == IW_CHAR || ty->width == IW_SHORT)
        return type_int();
    return ty;
}

/* C89 3.3.8 usual arithmetic conversions. */
Type *type_arith_convert(Type *a, Type *b)
{
    Type *signed_ty;
    Type *unsigned_ty;
    int rank_u;
    int rank_s;

    a = type_int_promote(a);
    b = type_int_promote(b);
    if (!a || !b || !type_is_integer(a) || !type_is_integer(b))
        return type_int();

    if (type_is_signed(a) == type_is_signed(b)) {
        if (type_int_rank(a) >= type_int_rank(b))
            return a;
        return b;
    }

    signed_ty = type_is_signed(a) ? a : b;
    unsigned_ty = type_is_signed(a) ? b : a;
    rank_s = type_int_rank(signed_ty);
    rank_u = type_int_rank(unsigned_ty);

    if (rank_u > rank_s)
        return unsigned_ty;
    if (rank_s > rank_u)
        return signed_ty;

    /* Same rank, mixed sign (e.g. int vs unsigned int). */
    return type_unsigned_same_width(signed_ty);
}

/* C89 3.1.5: hexadecimal/octal constant typing (no suffix). */
Type *type_classify_hex_constant(unsigned long v)
{
    if (v <= (unsigned long)INT_MAX)
        return type_int();
    if (v <= (unsigned long)UINT_MAX)
        return type_unsigned_int();
    if (v <= (unsigned long)LONG_MAX)
        return type_long();
    return type_unsigned_long();
}

Type *type_classify_octal_constant(unsigned long v)
{
    return type_classify_hex_constant(v);
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

static const char *int_base_name(Type *ty)
{
    switch (ty->width) {
    case IW_CHAR:  return "char";
    case IW_SHORT: return "short";
    case IW_INT:   return "int";
    case IW_LONG:  return "long";
    }
    return "integer";
}

const char *type_name(Type *ty)
{
    if (!ty)
        return "<null>";

    switch (ty->kind) {
    case TY_VOID: return "void";
    case TY_INT: {
        if (type_is_signed_char(ty))
            return "signed char";
        const char *base = int_base_name(ty);
        if (type_is_unsigned(ty)) {
            size_t n = strlen(base) + 10;
            char *buf = arena_alloc(n);
            snprintf(buf, n, "unsigned %s", base);
            return buf;
        }
        return base;
    }
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
    case TY_TYPEDEF_REF:
        return ty->ref_name ? ty->ref_name : "<typedef-ref>";
    case TY_STRUCT:
    case TY_UNION:
    case TY_ENUM: {
        const char *kw = ty->kind == TY_STRUCT ? "struct"
                       : ty->kind == TY_UNION  ? "union"
                                               : "enum";
        const char *tag = ty->tag ? ty->tag : "<anon>";
        size_t n = strlen(kw) + strlen(tag) + 2;
        char *buf = arena_alloc(n);
        snprintf(buf, n, "%s %s", kw, tag);
        return buf;
    }
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

/* UNDEFER: -Wsign-compare / narrowing unsigned-to-signed assignment warnings. */
