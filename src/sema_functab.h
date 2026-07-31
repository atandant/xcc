/* SPDX-License-Identifier: MIT */
#ifndef XCC_SEMA_FUNCTAB_H
#define XCC_SEMA_FUNCTAB_H

#include "ast.h"

/* Translation-unit registry of linked object and function entities. Lexical
 * visibility is tracked separately; file_visible exposes declarations made at
 * file scope without leaking block-scope extern declarations. */

typedef enum {
    FILESYM_FUNCTION,
    FILESYM_OBJECT,
} FileSymKind;

typedef struct FuncSym {
    char *name;
    Type *ty;
    FileSymKind kind;
    int defined;
    int tentative;
    int implicit;
    int referenced;
    Linkage linkage;
    GlobalObject *object;
    SourceLoc loc;
    SourceLoc reference_loc;
    int file_visible;
} FuncSym;

/* Clear the table before processing a new translation unit. */
void functab_reset(void);

FuncSym *functab_find(const char *name);
FuncSym *filesym_find(const char *name);
FuncSym *entity_find(const char *name);
FuncSym *functab_add(char *name, Type *ty, int defined, int implicit,
                     SourceLoc loc);
FuncSym *entity_declare(char *name, Type *ty, FileSymKind kind,
                        Linkage linkage, SourceLoc loc);

/* Merge a top-level declaration/definition into the table, emitting
 * redefinition / conflicting-type diagnostics per C89. */
void functab_register(Function *fn);
void functab_finalize(void);
void objecttab_register(GlobalObject *object);
void objecttab_finalize(void);

#endif /* XCC_SEMA_FUNCTAB_H */
