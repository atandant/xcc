/* SPDX-License-Identifier: MIT */
#ifndef XCC_SEMA_STRUCT_H
#define XCC_SEMA_STRUCT_H

#include "ast.h"

/* C89 struct tag namespace (3.5.2.1). One canonical Type * per tag name. */

void struct_tag_reset(void);

/* `struct Tag;` / `union Tag;` — forward declaration (incomplete). */
Type *struct_tag_forward(char *tag, SourceLoc loc);
Type *union_tag_forward(char *tag, SourceLoc loc);

/* `struct Tag { ... }` / `union Tag { ... }` — complete or error on redef. */
Type *struct_tag_define(char *tag, StructField *fields, SourceLoc loc);
Type *union_tag_define(char *tag, StructField *fields, SourceLoc loc);

/* Lookup an existing tag (complete or incomplete), or NULL. */
Type *struct_tag_lookup(const char *tag);

#endif /* XCC_SEMA_STRUCT_H */
