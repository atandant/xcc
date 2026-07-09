/* SPDX-License-Identifier: MIT */
#ifndef XCC_SEMA_ENUM_H
#define XCC_SEMA_ENUM_H

#include "ast.h"

/* C89 enum tags (3.5.2.3) and enumerator constants (3.5.2.2).
 * Enumerators are int constants living in the ordinary identifier namespace. */

void enum_reset(void);

/* `enum Tag` with no body: look up an existing tag or create an incomplete one. */
Type *enum_tag_forward(char *tag, SourceLoc loc);

/* `enum [Tag] { list }`: define the enumeration and register its constants.
 * `tag` may be NULL for an anonymous enum. Returns the completed TY_ENUM. */
Type *enum_tag_define(char *tag, Enumerator *list, SourceLoc loc);

/* Resolve an enumerator name to its int value; returns 1 on success. */
int enum_const_lookup(const char *name, long *out_value);

#endif /* XCC_SEMA_ENUM_H */
