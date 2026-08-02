/* SPDX-License-Identifier: MIT */
#include "sema_scope.h"
#include "sema_functab.h"
#include "diag.h"
#include "arena.h"
#include "type.h"

#include <string.h>

#define MAX_LOCALS 1024
#define MAX_SCOPES 256

typedef struct {
    int start_local;
} Scope;

static ScopeBinding locals[MAX_LOCALS];
static FrameLocal frame_locals[MAX_LOCALS];
static Scope scopes[MAX_SCOPES];
static int nlocals;
static int nframe_locals;
static int nscopes;
static int cur_offset; /* grows downward from the frame pointer, in bytes */

static int promotable_scalar_type(Type *ty)
{
    int size = type_size(ty);

    return type_is_scalar(ty) &&
           (size == 4 || size == 8 ||
            (size == 16 && type_is_floating(ty)));
}

void scope_reset(void)
{
    nlocals = 0;
    nframe_locals = 0;
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
    ScopeBinding *binding = scope_lookup_binding(name);

    if (!binding)
        return 0;
    if (out_offset)
        *out_offset = binding->offset;
    if (out_ty)
        *out_ty = binding->ty;
    return 1;
}

ScopeBinding *scope_lookup_binding(const char *name)
{
    for (int i = nlocals - 1; i >= 0; i--)
        if (strcmp(locals[i].name, name) == 0)
            return &locals[i];
    return NULL;
}

ScopeBinding *scope_lookup_binding_here(const char *name)
{
    int start = scopes[nscopes - 1].start_local;

    for (int i = nlocals - 1; i >= start; i--)
        if (strcmp(locals[i].name, name) == 0)
            return &locals[i];
    return NULL;
}

int scope_declared_here(const char *name)
{
    return scope_lookup_binding_here(name) != NULL;
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
    if (nframe_locals < MAX_LOCALS) {
        frame_locals[nframe_locals++] = (FrameLocal){
            .offset = cur_offset,
            .size = size,
            .promotable_scalar = promotable_scalar_type(ty),
        };
    }
    return cur_offset;
}

static FrameLocal *find_frame_local(int offset)
{
    for (int i = 0; i < nframe_locals; i++) {
        if (frame_locals[i].offset == offset)
            return &frame_locals[i];
    }
    return NULL;
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

void scope_bind(char *name, Type *ty, int offset, SourceLoc loc,
                int is_register)
{
    if (nlocals >= MAX_LOCALS) {
        diag_error_at(loc, "too many local variables in function");
        return;
    }
    locals[nlocals].name = name;
    locals[nlocals].ty = ty;
    locals[nlocals].offset = offset;
    locals[nlocals].loc = loc;
    locals[nlocals].kind = SCOPE_AUTO_OBJECT;
    locals[nlocals].symbol_name = NULL;
    locals[nlocals].entity = NULL;
    locals[nlocals].address_taken = 0;
    locals[nlocals].is_register = is_register;
    nlocals++;

    /* Stack-passed parameters have caller-frame offsets and therefore do not
       pass through scope_alloc_local().  Record them here as frame objects. */
    if (!find_frame_local(offset) && nframe_locals < MAX_LOCALS) {
        int size = type_size(ty);
        frame_locals[nframe_locals++] = (FrameLocal){
            .offset = offset,
            .size = size,
            .promotable_scalar = promotable_scalar_type(ty),
        };
    }
}

void scope_bind_static(char *name, Type *ty, char *symbol_name, SourceLoc loc)
{
    if (nlocals >= MAX_LOCALS) {
        diag_error_at(loc, "too many local declarations in function");
        return;
    }
    locals[nlocals++] = (ScopeBinding){
        .name = name,
        .ty = ty,
        .loc = loc,
        .kind = SCOPE_STATIC_OBJECT,
        .symbol_name = symbol_name,
    };
}

void scope_bind_linked(char *name, Type *ty, ScopeBindingKind kind,
                       FuncSym *entity, SourceLoc loc)
{
    if (nlocals >= MAX_LOCALS) {
        diag_error_at(loc, "too many local declarations in function");
        return;
    }
    locals[nlocals++] = (ScopeBinding){
        .name = name,
        .ty = ty,
        .loc = loc,
        .kind = kind,
        .symbol_name = entity ? entity->name : name,
        .entity = entity,
    };
}

void scope_mark_address_taken(const char *name)
{
    for (int i = nlocals - 1; i >= 0; i--) {
        if (strcmp(locals[i].name, name) == 0) {
            FrameLocal *frame_local;

            if (locals[i].kind != SCOPE_AUTO_OBJECT)
                return;
            locals[i].address_taken = 1;
            frame_local = find_frame_local(locals[i].offset);
            if (frame_local)
                frame_local->address_taken = 1;
            return;
        }
    }
}

int scope_is_register(const char *name)
{
    ScopeBinding *binding = scope_lookup_binding(name);

    return binding && binding->kind == SCOPE_AUTO_OBJECT &&
           binding->is_register;
}

int scope_offset_address_taken(int offset)
{
    FrameLocal *frame_local = find_frame_local(offset);
    return frame_local ? frame_local->address_taken : 0;
}

void scope_export_frame_locals(FrameLocal **out, int *out_n)
{
    FrameLocal *fl = arena_alloc((size_t)nframe_locals * sizeof(*fl));
    memcpy(fl, frame_locals, (size_t)nframe_locals * sizeof(*fl));
    *out = fl;
    *out_n = nframe_locals;
}

int scope_add_local(char *name, Type *ty, SourceLoc loc, int is_register)
{
    int off = scope_alloc_local(ty);
    scope_bind(name, ty, off, loc, is_register);
    return off;
}

int scope_frame_size(void)
{
    return -cur_offset;
}
