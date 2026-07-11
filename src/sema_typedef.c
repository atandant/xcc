/* SPDX-License-Identifier: MIT */
#include "sema_typedef.h"
#include "diag.h"

#include <string.h>

#define MAX_TYPEDEFS 1024
#define MAX_TYPEDEF_SCOPES 256

typedef struct {
    char *name;
    Type *ty;
    SourceLoc loc;
    int depth;
} TypedefEntry;

static TypedefEntry entries[MAX_TYPEDEFS];
static int scope_starts[MAX_TYPEDEF_SCOPES];
static int nentries;
static int cur_depth;

void typedef_reset(void)
{
    nentries = 0;
    cur_depth = 0;
}

void typedef_enter_scope(void)
{
    if (cur_depth >= MAX_TYPEDEF_SCOPES) {
        diag_error("too many nested typedef scopes");
        return;
    }
    scope_starts[cur_depth] = nentries;
    cur_depth++;
}

void typedef_leave_scope(void)
{
    if (cur_depth <= 0)
        return;
    cur_depth--;
    nentries = scope_starts[cur_depth];
}

int typedef_declared_here(const char *name)
{
    for (int i = nentries - 1; i >= 0; i--) {
        if (entries[i].depth != cur_depth)
            continue;
        if (strcmp(entries[i].name, name) == 0)
            return 1;
    }
    return 0;
}

int typedef_lookup_loc_here(const char *name, SourceLoc *out_loc)
{
    for (int i = nentries - 1; i >= 0; i--) {
        if (entries[i].depth != cur_depth)
            continue;
        if (strcmp(entries[i].name, name) == 0) {
            *out_loc = entries[i].loc;
            return 1;
        }
    }
    return 0;
}

Type *typedef_lookup(const char *name)
{
    for (int d = cur_depth; d >= 0; d--) {
        for (int i = nentries - 1; i >= 0; i--) {
            if (entries[i].depth != d)
                continue;
            if (strcmp(entries[i].name, name) == 0)
                return entries[i].ty;
        }
    }
    return NULL;
}

void typedef_bind(char *name, Type *ty, SourceLoc loc)
{
    if (!name || !ty)
        return;

    if (typedef_declared_here(name)) {
        SourceLoc prev;
        typedef_lookup_loc_here(name, &prev);
        diag_error_at(loc, "redefinition of typedef '%s'", name);
        diag_note_at(prev, "previous typedef of '%s' is here", name);
        return;
    }

    if (nentries >= MAX_TYPEDEFS) {
        diag_error_at(loc, "too many typedef names");
        return;
    }

    entries[nentries].name = name;
    entries[nentries].ty = ty;
    entries[nentries].loc = loc;
    entries[nentries].depth = cur_depth;
    nentries++;
}

void typedef_hide_name(char *name, SourceLoc loc)
{
    if (!name)
        return;
    if (nentries >= MAX_TYPEDEFS) {
        diag_error_at(loc, "too many ordinary identifiers in typedef scopes");
        return;
    }

    entries[nentries].name = name;
    entries[nentries].ty = NULL;
    entries[nentries].loc = loc;
    entries[nentries].depth = cur_depth;
    nentries++;
}

Type *typedef_resolve_spec(Type *spec, SourceLoc loc)
{
    Type *t;

    if (!spec || !type_is_typedef_ref(spec))
        return spec;

    t = typedef_lookup(spec->ref_name);
    if (!t) {
        diag_error_at(loc, "unknown type name '%s'", spec->ref_name);
        return type_int();
    }
    return t;
}

void typedef_declare(Type *base, Declarator *decl, SourceLoc loc)
{
    char *name;
    Type *spec;
    Type *ty;

    if (!decl)
        return;

    spec = typedef_resolve_spec(base, loc);
    if (!spec)
        return;

    name = declarator_name(decl);
    if (!name) {
        diag_error_at(loc, "typedef requires a name");
        return;
    }

    ty = type_apply_declarator(spec, decl, loc);
    typedef_bind(name, ty, loc);
}

Type *typedef_resolve_type(Type *ty, SourceLoc loc)
{
    Type *base;
    Type *rb;

    if (!ty)
        return ty;

    if (type_is_typedef_ref(ty))
        return typedef_resolve_type(typedef_resolve_spec(ty, loc), loc);

    if (type_is_pointer(ty)) {
        rb = typedef_resolve_type(ty->base, loc);
        return rb == ty->base ? ty : type_ptr(rb);
    }

    if (type_is_array(ty)) {
        base = typedef_resolve_type(ty->base, loc);
        return base == ty->base ? ty : type_array(base, ty->count);
    }

    if (type_is_record(ty) || type_is_enum(ty))
        return ty;

    return ty;
}
