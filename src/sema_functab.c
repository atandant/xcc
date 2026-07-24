/* SPDX-License-Identifier: MIT */
#include "sema_functab.h"
#include "diag.h"

#include <string.h>

#define MAX_FUNCS 4096

static FuncSym funcs[MAX_FUNCS];
static int nfuncs;

void functab_reset(void)
{
    nfuncs = 0;
}

FuncSym *filesym_find(const char *name)
{
    for (int i = 0; i < nfuncs; i++)
        if (strcmp(funcs[i].name, name) == 0)
            return &funcs[i];
    return NULL;
}

FuncSym *functab_find(const char *name)
{
    FuncSym *symbol = filesym_find(name);
    return symbol && symbol->kind == FILESYM_FUNCTION ? symbol : NULL;
}

FuncSym *functab_add(char *name, Type *ty, int defined, int implicit,
                     SourceLoc loc)
{
    if (nfuncs >= MAX_FUNCS) {
        diag_error_at(loc, "too many functions in translation unit");
        return NULL;
    }
    FuncSym *s = &funcs[nfuncs++];
    s->name = name;
    s->ty = ty;
    s->kind = FILESYM_FUNCTION;
    s->defined = defined;
    s->implicit = implicit;
    s->object = NULL;
    s->loc = loc;
    return s;
}

/* Merge a top-level declaration/definition into the function table, emitting
 * redefinition / conflicting-type errors per C89.
 *
 * type_same on two TY_FUNC types yields the C89 merge rule we want: a bare
 * `()` declaration is compatible with a prototype (only the return type is
 * compared), while two prototypes must agree on parameter types and count. */
void functab_register(Function *fn)
{
    FuncSym *s = filesym_find(fn->name);
    if (!s) {
        if (!functab_add(fn->name, fn->ty, fn->is_definition, 0, fn->loc))
            return;
        if (fn->is_definition && !fn->ty->prototyped)
            diag_warn(W_OLD_STYLE_DEFINITION, fn->loc,
                      "function '%s' defined without a prototype", fn->name);
        return;
    }

    if (s->kind != FILESYM_FUNCTION) {
        diag_error_at(fn->loc, "redeclared '%s' as different kind of symbol",
                      fn->name);
        diag_note_at(s->loc, "previous declaration of '%s' is here", fn->name);
        return;
    }

    if (fn->is_definition && s->defined) {
        diag_error_at(fn->loc, "redefinition of '%s'", fn->name);
        diag_note_at(s->loc, "previous definition of '%s' is here", fn->name);
    } else if (!type_same(s->ty, fn->ty)) {
        diag_error_at(fn->loc, "conflicting types for '%s'", fn->name);
        diag_note_at(fn->loc, "conflicting declaration has type '%s'",
                     type_func_sig(fn->ty));
        diag_note_at(s->loc, "previous declaration is here with type '%s'",
                     type_func_sig(s->ty));
    }

    /* A later prototype refines an earlier unprototyped declaration. */
    if (fn->ty->prototyped && !s->ty->prototyped)
        s->ty = fn->ty;
    if (fn->is_definition) {
        s->defined = 1;
        s->loc = fn->loc;
    }
}

void objecttab_register(GlobalObject *object)
{
    FuncSym *s = filesym_find(object->name);

    object->emit = 0;
    if (!s) {
        if (nfuncs >= MAX_FUNCS) {
            diag_error_at(object->loc,
                          "too many file-scope symbols in translation unit");
            return;
        }
        s = &funcs[nfuncs++];
        s->name = object->name;
        s->ty = object->ty;
        s->kind = FILESYM_OBJECT;
        s->defined = object->init != NULL;
        s->implicit = 0;
        s->object = object;
        s->loc = object->loc;
        object->emit = 1;
        return;
    }
    if (s->kind != FILESYM_OBJECT) {
        diag_error_at(object->loc,
                      "redeclared '%s' as different kind of symbol",
                      object->name);
        diag_note_at(s->loc, "previous declaration of '%s' is here",
                     object->name);
        return;
    }
    if (!type_same(s->ty, object->ty)) {
        diag_error_at(object->loc, "conflicting types for '%s'", object->name);
        diag_note_at(s->loc, "previous declaration of '%s' is here",
                     object->name);
        return;
    }
    if (object->init) {
        if (s->defined) {
            diag_error_at(object->loc, "redefinition of '%s'", object->name);
            diag_note_at(s->loc, "previous definition of '%s' is here",
                         object->name);
            return;
        }
        if (s->object)
            s->object->emit = 0;
        s->object = object;
        s->defined = 1;
        s->loc = object->loc;
        object->emit = 1;
    }
}
