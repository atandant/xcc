/* SPDX-License-Identifier: MIT */
#ifndef XCC_SEMA_FUNCTAB_H
#define XCC_SEMA_FUNCTAB_H

#include "ast.h"

/* Translation-unit table of function declarations and definitions. sema
 * consults it to type-check calls and to diagnose redefinitions and
 * conflicting declarations. */

typedef enum {
    FILESYM_FUNCTION,
    FILESYM_OBJECT,
} FileSymKind;

typedef struct {
    char *name;
    Type *ty;
    FileSymKind kind;
    int defined;
    int implicit;
    GlobalObject *object;
    SourceLoc loc;
} FuncSym;

/* Clear the table before processing a new translation unit. */
void functab_reset(void);

FuncSym *functab_find(const char *name);
FuncSym *filesym_find(const char *name);
FuncSym *functab_add(char *name, Type *ty, int defined, int implicit,
                     SourceLoc loc);

/* Merge a top-level declaration/definition into the table, emitting
 * redefinition / conflicting-type diagnostics per C89. */
void functab_register(Function *fn);
void objecttab_register(GlobalObject *object);

#endif /* XCC_SEMA_FUNCTAB_H */
