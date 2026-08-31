/* SPDX-License-Identifier: MIT */
#include "sema_struct.h"
#include "sema_enum.h"
#include "diag.h"
#include "arena.h"
#include "intconst.h"

#include <limits.h>
#include <string.h>

#define MAX_STRUCT_TAGS 256
#define MAX_STRUCT_SCOPES 256
#define MAX_STRUCT_MEMBERS 127
#define MAX_STRUCT_NESTING 6

typedef struct {
    char *tag;
    Type *ty;
    SourceLoc loc;
    int is_union;   /* struct and union share one tag namespace (C89 3.1.2.3) */
} StructTagEntry;

static StructTagEntry tags[MAX_STRUCT_TAGS];
static int scope_starts[MAX_STRUCT_SCOPES];
static int ntags;
static int nscopes;

void struct_tag_reset(void)
{
    ntags = 0;
    nscopes = 0;
}

void struct_tag_enter_scope(void)
{
    if (nscopes >= MAX_STRUCT_SCOPES) {
        diag_error("too many nested struct tag scopes");
        return;
    }
    scope_starts[nscopes++] = ntags;
}

void struct_tag_leave_scope(void)
{
    if (nscopes <= 0)
        return;
    ntags = scope_starts[--nscopes];
}

Type *struct_tag_lookup(const char *tag)
{
    for (int i = ntags - 1; i >= 0; i--) {
        if (strcmp(tags[i].tag, tag) == 0)
            return tags[i].ty;
    }
    return NULL;
}

static Type *struct_type_new(char *tag, int is_union)
{
    Type *t = arena_alloc_zeroed(sizeof(Type));
    t->kind = is_union ? TY_UNION : TY_STRUCT;
    t->tag = tag;
    t->is_complete = 0;
    t->size = 0;
    t->align = 1;
    return t;
}

static void struct_tag_register(char *tag, Type *ty, int is_union, SourceLoc loc)
{
    if (ntags >= MAX_STRUCT_TAGS) {
        diag_error_at(loc, "too many struct tags");
        return;
    }
    tags[ntags].tag = tag;
    tags[ntags].ty = ty;
    tags[ntags].loc = loc;
    tags[ntags].is_union = is_union;
    ntags++;
}

/* Return the existing entry for `tag`, or NULL. Reports a kind mismatch
 * (`struct S` vs `union S`) since both share the tag namespace. */
static StructTagEntry *struct_tag_entry(const char *tag, int is_union,
                                        SourceLoc loc, int current_scope_only,
                                        int *mismatch)
{
    int first = current_scope_only && nscopes > 0
              ? scope_starts[nscopes - 1] : 0;

    if (mismatch)
        *mismatch = 0;
    for (int i = ntags - 1; i >= first; i--) {
        if (strcmp(tags[i].tag, tag) != 0)
            continue;
        if (tags[i].is_union != is_union) {
            diag_error_at(loc,
                          "use of '%s' with tag type that does not match "
                          "previous declaration", tag);
            diag_note_at(tags[i].loc, "previous declaration of '%s' is here",
                         tag);
            if (mismatch)
                *mismatch = 1;
        }
        return &tags[i];
    }
    return NULL;
}

static int struct_ice_lookup(const char *name, long *out, Type **out_ty,
                             void *ctx)
{
    (void)ctx;
    if (!enum_const_lookup(name, out))
        return 0;
    if (out_ty)
        *out_ty = type_int();
    return 1;
}

static int ice_eval(Node *n, long *out)
{
    return int_const_eval(n, struct_ice_lookup, int_const_sizeof_type, NULL,
                          out, NULL);
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
    if (type_is_record(ty) && !type_struct_is_complete(ty)) {
        diag_error_at(loc, "struct member has incomplete type '%s'",
                      type_name(ty));
        return 0;
    }
    if (type_is_array(ty) && type_array_elem(ty) &&
        type_is_record(type_array_elem(ty)) &&
        !type_struct_is_complete(type_array_elem(ty))) {
        diag_error_at(loc, "array of incomplete struct type is not allowed");
        return 0;
    }
    return 1;
}

static int struct_nest_depth(Type *ty)
{
    int max = 0;

    if (!type_is_record(ty) || !type_struct_is_complete(ty))
        return 0;
    ty = type_unqualified(ty);

    for (int i = 0; i < ty->nmembers; i++) {
        Type *mty = ty->members[i].ty;
        int d = 0;

        if (type_is_record(mty))
            d = 1 + struct_nest_depth(mty);
        else if (type_is_array(mty)) {
            Type *elem = type_array_elem(mty);

            if (elem && type_is_record(elem))
                d = 1 + struct_nest_depth(elem);
        }
        if (d > max)
            max = d;
    }
    return max;
}

static int check_struct_translation_limits(Type *ty, SourceLoc loc)
{
    if (ty->nmembers > MAX_STRUCT_MEMBERS) {
        diag_error_at(loc,
                      "struct exceeds translation limit of %d members",
                      MAX_STRUCT_MEMBERS);
        return 0;
    }
    if (struct_nest_depth(ty) > MAX_STRUCT_NESTING) {
        diag_error_at(loc,
                      "struct nesting exceeds translation limit of %d levels",
                      MAX_STRUCT_NESTING);
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

static Type *record_tag_forward(char *tag, int is_union, SourceLoc loc)
{
    StructTagEntry *e = struct_tag_entry(tag, is_union, loc, 0, NULL);
    Type *ty;

    if (e)
        return e->ty;

    ty = struct_type_new(tag, is_union);
    struct_tag_register(tag, ty, is_union, loc);
    return ty;
}

static Type *record_tag_define(char *tag, StructField *fields, int is_union,
                               SourceLoc loc)
{
    int mismatch = 0;
    StructTagEntry *e = tag
                      ? struct_tag_entry(tag, is_union, loc, 1, &mismatch)
                      : NULL;
    Type *ty = (e && !mismatch) ? e->ty : NULL;
    const char *kw = is_union ? "union" : "struct";
    Member *members;
    int nmembers = 0;
    int i;

    members = fields_to_members(fields, &nmembers, loc);
    for (i = 0; i < nmembers; i++) {
        if (!members[i].is_bitfield && !member_type_ok(members[i].ty, loc)) {
            if (ty)
                return ty;
            if (tag)
                return record_tag_forward(tag, is_union, loc);
            return struct_type_new(NULL, is_union);
        }
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
            diag_error_at(loc, "redefinition of %s '%s'", kw, tag);
            diag_note_at(prev, "previous definition of %s '%s' is here", kw, tag);
        }
        return ty;
    }

    if (!ty) {
        ty = struct_type_new(tag, is_union);
        if (tag)
            struct_tag_register(tag, ty, is_union, loc);
    }

    ty->members = members;
    ty->nmembers = nmembers;
    ty->is_complete = 1;
    type_struct_layout(ty);
    if (!check_struct_translation_limits(ty, loc))
        ty->is_complete = 0;
    return ty;
}

Type *struct_tag_forward(char *tag, SourceLoc loc)
{
    return record_tag_forward(tag, 0, loc);
}

Type *union_tag_forward(char *tag, SourceLoc loc)
{
    return record_tag_forward(tag, 1, loc);
}

Type *struct_tag_define(char *tag, StructField *fields, SourceLoc loc)
{
    return record_tag_define(tag, fields, 0, loc);
}

Type *union_tag_define(char *tag, StructField *fields, SourceLoc loc)
{
    return record_tag_define(tag, fields, 1, loc);
}
