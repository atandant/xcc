/* SPDX-License-Identifier: MIT */
#ifndef XCC_TYPE_H
#define XCC_TYPE_H

/* The semantic type representation for xcc 0.0.1.3.
 *
 * Types live in sema's conceptual space: the parser builds them from syntax
 * and attaches them to AST nodes, sema checks them, and codegen asks them how
 * to load/store/compare. Keep this module small and recursive. */

typedef enum {
    TY_VOID,
    TY_CHAR,
    TY_INT,
    TY_PTR,
    TY_FUNC
} TypeKind;

typedef struct Type Type;
struct Type {
    TypeKind kind;
    int size;          /* object size in bytes (0 for void/func)        */
    int align;         /* alignment in bytes                            */

    Type *base;        /* TY_PTR: pointee type                          */

    Type *ret;         /* TY_FUNC: return type                          */
    Type **params;     /* TY_FUNC: parameter types                      */
    int nparams;       /* TY_FUNC: parameter count                      */
    int prototyped;    /* TY_FUNC: 0 = bare () unspecified args          */
};

/* Builtin singleton constructors. */
Type *type_void(void);
Type *type_char(void);
Type *type_int(void);

/* Derived-type constructors (arena allocated). */
Type *type_ptr(Type *base);
Type *type_func(Type *ret, Type **params, int nparams, int prototyped);

/* Predicates. */
int type_is_void(Type *ty);
int type_is_char(Type *ty);
int type_is_integer(Type *ty);
int type_is_pointer(Type *ty);
int type_is_scalar(Type *ty);
int type_is_object(Type *ty);

/* Relations. */
int type_same(Type *a, Type *b);
int type_compatible(Type *a, Type *b);
int type_assignable(Type *dst, Type *src);

/* Queries. */
int type_size(Type *ty);
int type_align(Type *ty);
const char *type_name(Type *ty);
const char *type_func_sig(Type *ty);

#endif /* XCC_TYPE_H */
