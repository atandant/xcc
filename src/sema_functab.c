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

FuncSym *entity_find(const char *name)
{
    for (int i = 0; i < nfuncs; i++)
        if (strcmp(funcs[i].name, name) == 0)
            return &funcs[i];
    return NULL;
}

FuncSym *filesym_find(const char *name)
{
    FuncSym *symbol = entity_find(name);
    return symbol && symbol->file_visible ? symbol : NULL;
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
    s->tentative = 0;
    s->implicit = implicit;
    s->referenced = 0;
    s->linkage = LINKAGE_EXTERNAL;
    s->object = NULL;
    s->loc = loc;
    s->reference_loc = loc;
    s->file_visible = 1;
    return s;
}

FuncSym *entity_declare(char *name, Type *ty, FileSymKind kind,
                        Linkage linkage, SourceLoc loc)
{
    FuncSym *s = entity_find(name);

    if (!s) {
        if (nfuncs >= MAX_FUNCS) {
            diag_error_at(loc, "too many linked symbols in translation unit");
            return NULL;
        }
        s = &funcs[nfuncs++];
        *s = (FuncSym){
            .name = name,
            .ty = ty,
            .kind = kind,
            .linkage = linkage,
            .loc = loc,
            .reference_loc = loc,
        };
        return s;
    }
    if (s->kind != kind) {
        diag_error_at(loc, "redeclared '%s' as different kind of symbol", name);
        diag_note_at(s->loc, "previous declaration of '%s' is here", name);
        return s;
    }
    if (s->linkage != linkage) {
        diag_error_at(loc, "conflicting linkage for '%s'", name);
        diag_note_at(s->loc, "previous declaration of '%s' is here", name);
        return s;
    }
    if (!type_same(s->ty, ty)) {
        diag_error_at(loc, "conflicting types for '%s'", name);
        diag_note_at(s->loc, "previous declaration of '%s' is here", name);
        return s;
    }
    if (kind == FILESYM_FUNCTION && ty->prototyped && !s->ty->prototyped)
        s->ty = ty;
    if (kind == FILESYM_OBJECT && !type_is_complete(s->ty) &&
        type_is_complete(ty))
        s->ty = ty;
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
    FuncSym *s = entity_find(fn->name);
    Linkage linkage;

    if (fn->storage == STORAGE_STATIC)
        linkage = LINKAGE_INTERNAL;
    else if (s && s->kind == FILESYM_FUNCTION)
        linkage = s->linkage;
    else
        linkage = LINKAGE_EXTERNAL;
    fn->linkage = linkage;
    if (s)
        s->file_visible = 1;

    if (!s) {
        s = functab_add(fn->name, fn->ty, fn->is_definition, 0, fn->loc);
        if (!s)
            return;
        s->linkage = linkage;
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

    if (s->linkage != linkage) {
        diag_error_at(fn->loc, "conflicting linkage for '%s'", fn->name);
        diag_note_at(s->loc, "previous declaration of '%s' is here", fn->name);
        return;
    }

    if (fn->is_definition && s->defined) {
        diag_error_at(fn->loc, "redefinition of '%s'", fn->name);
        diag_note_at(s->loc, "previous definition of '%s' is here", fn->name);
        return;
    } else if (!type_same(s->ty, fn->ty)) {
        diag_error_at(fn->loc, "conflicting types for '%s'", fn->name);
        diag_note_at(fn->loc, "conflicting declaration has type '%s'",
                     type_func_sig(fn->ty));
        diag_note_at(s->loc, "previous declaration is here with type '%s'",
                     type_func_sig(s->ty));
        return;
    }

    /* A later prototype refines an earlier unprototyped declaration. */
    if (fn->ty->prototyped && !s->ty->prototyped)
        s->ty = fn->ty;
    if (fn->is_definition) {
        s->defined = 1;
        s->loc = fn->loc;
    }
}

void functab_finalize(void)
{
    for (int i = 0; i < nfuncs; i++) {
        FuncSym *s = &funcs[i];

        if (s->kind == FILESYM_FUNCTION &&
            s->linkage == LINKAGE_INTERNAL && s->referenced && !s->defined) {
            diag_error_at(s->reference_loc,
                          "static function '%s' is used but never defined",
                          s->name);
            diag_note_at(s->loc, "static declaration of '%s' is here", s->name);
        }
    }
}

void objecttab_register(GlobalObject *object)
{
    FuncSym *s = entity_find(object->name);
    Linkage linkage;

    object->emit = 0;
    if (object->storage == STORAGE_EXTERN && s && s->kind == FILESYM_OBJECT)
        linkage = s->linkage;
    else
        linkage = object->storage == STORAGE_STATIC
            ? LINKAGE_INTERNAL : LINKAGE_EXTERNAL;
    object->linkage = linkage;
    if (s)
        s->file_visible = 1;

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
        s->defined = object->decl_kind == OBJECT_DEFINITION;
        s->tentative = object->decl_kind == OBJECT_TENTATIVE;
        s->implicit = 0;
        s->referenced = 0;
        s->linkage = linkage;
        s->object = object->decl_kind == OBJECT_DECLARATION ? NULL : object;
        s->loc = object->loc;
        s->reference_loc = object->loc;
        s->file_visible = 1;
        object->emit = object->decl_kind != OBJECT_DECLARATION;
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
    if (s->linkage != linkage) {
        diag_error_at(object->loc, "conflicting linkage for '%s'", object->name);
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
    if (!type_is_complete(s->ty) && type_is_complete(object->ty)) {
        s->ty = object->ty;
        if (s->object)
            s->object->ty = object->ty;
    }
    if (object->decl_kind == OBJECT_DEFINITION) {
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
        s->tentative = 0;
        s->loc = object->loc;
        object->emit = 1;
    } else if (object->decl_kind == OBJECT_TENTATIVE && !s->defined) {
        s->tentative = 1;
        if (!s->object || (!type_is_complete(s->object->ty) &&
                           type_is_complete(object->ty))) {
            if (s->object)
                s->object->emit = 0;
            s->object = object;
            object->emit = 1;
        }
    }
}

void objecttab_finalize(void)
{
    for (int i = 0; i < nfuncs; i++) {
        FuncSym *s = &funcs[i];
        GlobalObject *object;

        if (s->kind != FILESYM_OBJECT || s->defined || !s->tentative)
            continue;
        object = s->object;
        if (!object)
            continue;
        if (!type_is_complete(object->ty) && type_is_array(object->ty) &&
            type_array_count(object->ty) == 0 &&
            type_is_complete(type_array_elem(object->ty))) {
            object->ty->count = 1;
            object->ty->size = type_size(type_array_elem(object->ty));
            s->ty = object->ty;
        }
        object->emit = 1;
    }
}
