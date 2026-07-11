/* SPDX-License-Identifier: MIT */
#ifndef XCC_SEMA_TYPEDEF_H
#define XCC_SEMA_TYPEDEF_H

#include "ast.h"

/* typedef name environment: file scope + nested block scopes (parse + sema).
 * Typedef names live in the ordinary identifier namespace (C89 3.1.2.3). */

void typedef_reset(void);

void typedef_enter_scope(void);
void typedef_leave_scope(void);

/* Bind `name` as synonym for `ty` in the current scope. Errors on redefinition. */
void typedef_bind(char *name, Type *ty, SourceLoc loc);

/* Record an ordinary identifier that hides an outer typedef name. */
void typedef_hide_name(char *name, SourceLoc loc);

/* Resolve a typedef name to its Type *, or NULL if not a typedef. */
Type *typedef_lookup(const char *name);

/* Resolve TY_TYPEDEF_REF specifier to concrete Type * (or fallback on error). */
Type *typedef_resolve_spec(Type *spec, SourceLoc loc);

/* Recursively resolve typedef refs inside pointers and arrays. */
Type *typedef_resolve_type(Type *ty, SourceLoc loc);

int typedef_declared_here(const char *name);
int typedef_lookup_loc_here(const char *name, SourceLoc *out_loc);

/* Apply declarator to base and bind each typedef name (comma lists later). */
void typedef_declare(Type *base, Declarator *decl, SourceLoc loc);

#endif /* XCC_SEMA_TYPEDEF_H */
