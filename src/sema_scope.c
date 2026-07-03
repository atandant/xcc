/* SPDX-License-Identifier: MIT */
#include "sema_scope.h"
#include "diag.h"

#include <string.h>

#define MAX_LOCALS 1024
#define MAX_SCOPES 256

typedef struct {
    char *name;
    Type *ty;
    int offset;
    SourceLoc loc;
} Local;

typedef struct {
    int start_local;
} Scope;

static Local locals[MAX_LOCALS];
static Scope scopes[MAX_SCOPES];
static int nlocals;
static int nscopes;
static int cur_offset; /* grows downward from %rbp, in bytes */

void scope_reset(void)
{
    nlocals = 0;
    nscopes = 0;
    cur_offset = 0;
}

void enter_scope(void)
{
    if (nscopes >= MAX_SCOPES) {
        diag_error("too many nested scopes");
        return;
    }
    scopes[nscopes].start_local = nlocals;
    nscopes++;
}

void leave_scope(void)
{
    if (nscopes <= 0)
        return;
    nscopes--;
    nlocals = scopes[nscopes].start_local;
}

int scope_lookup(const char *name, int *out_offset, Type **out_ty)
{
    for (int i = nlocals - 1; i >= 0; i--) {
        if (strcmp(locals[i].name, name) == 0) {
            if (out_offset)
                *out_offset = locals[i].offset;
            if (out_ty)
                *out_ty = locals[i].ty;
            return 1;
        }
    }
    return 0;
}

int scope_declared_here(const char *name)
{
    int start = scopes[nscopes - 1].start_local;
    for (int i = start; i < nlocals; i++)
        if (strcmp(locals[i].name, name) == 0)
            return 1;
    return 0;
}

/* Reserve stack space for a local object (type-aware size and alignment). */
static int align_down(int off, int align)
{
    unsigned u = (unsigned)(-off);
    unsigned rem = u % (unsigned)align;

    if (rem != 0)
        off -= (int)rem;
    return off;
}

int scope_alloc_local(Type *ty)
{
    int size = type_size(ty);
    int align = type_align(ty);

    if (size < 1) {
        diag_error("invalid object size for local variable");
        size = 1;
    }
    cur_offset -= size;
    if (align > 1)
        cur_offset = align_down(cur_offset, align);
    return cur_offset;
}

int scope_lookup_loc_here(const char *name, SourceLoc *out_loc)
{
    int start = scopes[nscopes - 1].start_local;

    for (int i = nlocals - 1; i >= start; i--) {
        if (strcmp(locals[i].name, name) == 0) {
            *out_loc = locals[i].loc;
            return 1;
        }
    }
    return 0;
}

void scope_bind(char *name, Type *ty, int offset, SourceLoc loc)
{
    if (nlocals >= MAX_LOCALS) {
        diag_error_at(loc, "too many local variables in function");
        return;
    }
    locals[nlocals].name = name;
    locals[nlocals].ty = ty;
    locals[nlocals].offset = offset;
    locals[nlocals].loc = loc;
    nlocals++;
}

int scope_add_local(char *name, Type *ty, SourceLoc loc)
{
    int off = scope_alloc_local(ty);
    scope_bind(name, ty, off, loc);
    return off;
}

int scope_frame_size(void)
{
    return -cur_offset;
}
