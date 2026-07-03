/* SPDX-License-Identifier: MIT */
#ifndef XCC_SEMA_SCOPE_H
#define XCC_SEMA_SCOPE_H

#include "ast.h"

/* Per-function local environment: a stack of nested scopes holding named
 * objects, plus the downward-growing %rbp offsets assigned to them. sema owns
 * this state for the function currently being resolved. */

/* Begin a fresh function frame (clears all scopes, locals, and offsets). */
void scope_reset(void);

void enter_scope(void);
void leave_scope(void);

/* Resolve a name against every enclosing scope (innermost first). */
int scope_lookup(const char *name, int *out_offset, Type **out_ty);

/* Was `name` already declared in the innermost scope? */
int scope_declared_here(const char *name);

/* Source location of `name`'s declaration in the innermost scope. */
int scope_lookup_loc_here(const char *name, SourceLoc *out_loc);

/* Reserve a type-sized, type-aligned stack slot and return its offset. */
int scope_alloc_local(Type *ty);

/* Bind a name to an already-allocated offset (used for register params). */
void scope_bind(char *name, Type *ty, int offset, SourceLoc loc);

/* Allocate a slot and bind `name` to it in one step; returns the offset. */
int scope_add_local(char *name, Type *ty, SourceLoc loc);

/* Bytes of stack reserved for locals so far (non-negative). */
int scope_frame_size(void);

#endif /* XCC_SEMA_SCOPE_H */
