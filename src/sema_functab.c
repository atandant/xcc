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

FuncSym *functab_find(const char *name)
{
    for (int i = 0; i < nfuncs; i++)
        if (strcmp(funcs[i].name, name) == 0)
            return &funcs[i];
    return NULL;
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
    s->defined = defined;
    s->implicit = implicit;
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
    FuncSym *s = functab_find(fn->name);
    if (!s) {
        if (!functab_add(fn->name, fn->ty, fn->is_definition, 0, fn->loc))
            return;
        if (fn->is_definition && !fn->ty->prototyped)
            diag_warn(W_OLD_STYLE_FUNCTION_DEFINITION, fn->loc,
                      "function '%s' defined without a prototype", fn->name);
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
