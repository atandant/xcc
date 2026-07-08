/* SPDX-License-Identifier: MIT */
#include "sema_struct.h"
#include "diag.h"
#include "arena.h"
#include "intconst.h"

#include <limits.h>
#include <string.h>

#define MAX_STRUCT_TAGS 256

typedef struct {
    char *tag;
    Type *ty;
    SourceLoc loc;
} StructTagEntry;

static StructTagEntry tags[MAX_STRUCT_TAGS];
static int ntags;

void struct_tag_reset(void)
{
    ntags = 0;
}

Type *struct_tag_lookup(const char *tag)
{
    for (int i = 0; i < ntags; i++) {
        if (strcmp(tags[i].tag, tag) == 0)
            return tags[i].ty;
    }
    return NULL;
}

static Type *struct_type_new(char *tag)
{
    Type *t = arena_alloc_zeroed(sizeof(Type));
    t->kind = TY_STRUCT;
    t->tag = tag;
    t->is_complete = 0;
    t->size = 0;
    t->align = 1;
    return t;
}

static void struct_tag_register(char *tag, Type *ty, SourceLoc loc)
{
    if (ntags >= MAX_STRUCT_TAGS) {
        diag_error_at(loc, "too many struct tags");
        return;
    }
    tags[ntags].tag = tag;
    tags[ntags].ty = ty;
    tags[ntags].loc = loc;
    ntags++;
}

static int ice_eval(Node *n, long *out)
{
    if (!n)
        return 0;

    switch (n->kind) {
    case ND_NUM:
        *out = n->val;
        return 1;
    case ND_NEG:
        if (!ice_eval(n->operand, out))
            return 0;
        return int_const_neg(*out, out);
    case ND_BINOP:
        if (n->op == OP_COMMA)
            return ice_eval(n->rhs, out);
        {
            long l, r;
            if (!ice_eval(n->lhs, &l) || !ice_eval(n->rhs, &r))
                return 0;
            return int_const_binop(n->op, l, r, out);
        }
    default:
        return 0;
    }
}

static int is_bitfield_spec(Type *ty)
{
    if (!ty || !type_is_integer(ty))
        return 0;
    return ty->width == IW_INT;
}

static int member_type_ok(Type *ty, SourceLoc loc)
{
    if (!ty)
        return 0;
    if (type_is_void(ty)) {
        diag_error_at(loc, "struct member cannot have void type");
        return 0;
    }
    if (ty->kind == TY_FUNC) {
        diag_error_at(loc, "struct member cannot be a function");
        return 0;
    }
    if (type_is_struct(ty) && !type_struct_is_complete(ty)) {
        diag_error_at(loc, "struct member has incomplete type '%s'",
                      type_name(ty));
        return 0;
    }
    if (type_is_array(ty) && type_array_elem(ty) &&
        type_is_struct(type_array_elem(ty)) &&
        !type_struct_is_complete(type_array_elem(ty))) {
        diag_error_at(loc, "array of incomplete struct type is not allowed");
        return 0;
    }
    return 1;
}

static void member_append(Member **members, int *nmembers, Member *m)
{
    Member *arr;
    int n = *nmembers;

    arr = arena_alloc((size_t)(n + 1) * sizeof(*arr));
    if (n > 0)
        memcpy(arr, *members, (size_t)n * sizeof(*arr));
    arr[n] = *m;
    *members = arr;
    (*nmembers)++;
}

static Member *fields_to_members(StructField *fields, int *out_n, SourceLoc loc)
{
    Member *members = NULL;
    int nmembers = 0;
    StructField *f;

    for (f = fields; f; f = f->next) {
        char *name = f->decl ? declarator_name(f->decl) : NULL;
        Type *mty;
        Member m = {0};
        long width = 0;

        if (!f->decl || !f->decl->bit_width_expr) {
            if (!name) {
                diag_error_at(f->loc, "struct member requires a name");
                continue;
            }
            mty = type_apply_declarator(f->spec, f->decl, f->loc);
            if (!member_type_ok(mty, f->loc))
                continue;
        } else {
            if (!f->spec || !is_bitfield_spec(f->spec)) {
                diag_error_at(f->loc, "bit-field has invalid type");
                continue;
            }
            if (!ice_eval(f->decl->bit_width_expr, &width)) {
                diag_error_at(f->loc,
                              "bit-field width is not an integer constant expression");
                continue;
            }
            if (width < 0) {
                diag_error_at(f->loc, "bit-field width is negative");
                continue;
            }
            if (width == 0 && name) {
                diag_error_at(f->loc, "zero width for bit-field '%s'", name);
                continue;
            }
            if (width > (long)(sizeof(unsigned int) * 8)) {
                diag_error_at(f->loc, "bit-field width exceeds int size");
                continue;
            }
            mty = f->decl ? type_apply_declarator(f->spec, f->decl, f->loc) : f->spec;
        }

        m.name = name;
        m.ty = mty;
        m.is_bitfield = f->decl && f->decl->bit_width_expr != NULL;
        m.bit_width = (int)width;
        m.bit_offset = 0;
        member_append(&members, &nmembers, &m);
    }

    if (out_n)
        *out_n = nmembers;
    (void)loc;
    return members;
}

static int members_compatible(Member *a, int na, Member *b, int nb)
{
    if (na != nb)
        return 0;
    for (int i = 0; i < na; i++) {
        if ((a[i].name == NULL) != (b[i].name == NULL))
            return 0;
        if (a[i].name && strcmp(a[i].name, b[i].name) != 0)
            return 0;
        if (!type_same(a[i].ty, b[i].ty))
            return 0;
        if (a[i].is_bitfield != b[i].is_bitfield)
            return 0;
        if (a[i].is_bitfield &&
            (a[i].bit_width != b[i].bit_width ||
             a[i].bit_offset != b[i].bit_offset))
            return 0;
    }
    return 1;
}

Type *struct_tag_forward(char *tag, SourceLoc loc)
{
    Type *ty = struct_tag_lookup(tag);
    if (ty)
        return ty;

    ty = struct_type_new(tag);
    struct_tag_register(tag, ty, loc);
    return ty;
}

Type *struct_tag_define(char *tag, StructField *fields, SourceLoc loc)
{
    Type *ty = struct_tag_lookup(tag);
    Member *members;
    int nmembers = 0;
    int i;

    members = fields_to_members(fields, &nmembers, loc);
    for (i = 0; i < nmembers; i++) {
        if (!members[i].is_bitfield && !member_type_ok(members[i].ty, loc))
            return ty ? ty : struct_tag_forward(tag, loc);
    }

    if (ty && type_struct_is_complete(ty)) {
        if (!members_compatible(ty->members, ty->nmembers, members, nmembers)) {
            SourceLoc prev = loc;
            for (int j = 0; j < ntags; j++) {
                if (tags[j].ty == ty) {
                    prev = tags[j].loc;
                    break;
                }
            }
            diag_error_at(loc, "redefinition of struct '%s'", tag);
            diag_note_at(prev, "previous definition of struct '%s' is here", tag);
        }
        return ty;
    }

    if (!ty) {
        ty = struct_type_new(tag);
        struct_tag_register(tag, ty, loc);
    }

    ty->members = members;
    ty->nmembers = nmembers;
    ty->is_complete = 1;
    type_struct_layout(ty);
    return ty;
}
