/* SPDX-License-Identifier: MIT */
#ifndef XCC_SEMA_SCOPE_H
#define XCC_SEMA_SCOPE_H

#include "ast.h"

typedef struct FuncSym FuncSym;

typedef enum {
    SCOPE_AUTO_OBJECT,
    SCOPE_STATIC_OBJECT,
    SCOPE_LINKED_OBJECT,
    SCOPE_FUNCTION,
} ScopeBindingKind;

typedef struct {
    char *name;
    Type *ty;
    SourceLoc loc;
    ScopeBindingKind kind;
    int offset;
    char *symbol_name;
    FuncSym *entity;
    int address_taken;
    int is_register;
} ScopeBinding;

/* Per-function lexical environment: a stack of nested scopes holding
 * automatic/static objects and linked object/function bindings. Stack frame
 * offsets remain meaningful only for automatic objects. */

/* Begin a fresh function frame (clears all scopes, locals, and offsets). */
void scope_reset(void);

void enter_scope(void);
void leave_scope(void);

/* Resolve a name against every enclosing scope (innermost first). */
int scope_lookup(const char *name, int *out_offset, Type **out_ty);
ScopeBinding *scope_lookup_binding(const char *name);
ScopeBinding *scope_lookup_binding_here(const char *name);

/* Was `name` already declared in the innermost scope? */
int scope_declared_here(const char *name);

/* Source location of `name`'s declaration in the innermost scope. */
int scope_lookup_loc_here(const char *name, SourceLoc *out_loc);

/* Reserve a type-sized, type-aligned stack slot and return its offset. */
int scope_alloc_local(Type *ty);

/* Bind a name to an already-allocated offset (used for ABI register params). */
void scope_bind(char *name, Type *ty, int offset, SourceLoc loc,
                int is_register);
void scope_bind_static(char *name, Type *ty, char *symbol_name, SourceLoc loc);
void scope_bind_linked(char *name, Type *ty, ScopeBindingKind kind,
                       FuncSym *entity, SourceLoc loc);

/* Allocate a slot and bind `name` to it in one step; returns the offset. */
int scope_add_local(char *name, Type *ty, SourceLoc loc, int is_register);

/* Bytes of stack reserved for locals so far (non-negative). */
int scope_frame_size(void);

void scope_mark_address_taken(const char *name);
int scope_is_register(const char *name);
int scope_offset_address_taken(int offset);
void scope_export_frame_locals(FrameLocal **out, int *out_n);

#endif /* XCC_SEMA_SCOPE_H */
