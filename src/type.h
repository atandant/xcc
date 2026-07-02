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
    TY_INT,     /* all integer types: char, int, long, later short/unsigned */
    TY_PTR,
    TY_ARRAY,
    TY_FUNC
} TypeKind;

typedef enum {
    IW_CHAR,
    IW_INT,
    IW_LONG
} IntWidth;

typedef enum {
    IS_SIGNED
} IntSign;

typedef struct Type Type;
struct Type {
    TypeKind kind;
    IntWidth width;    /* TY_INT: char / int / long                    */
    IntSign sign;      /* TY_INT: signedness (unsigned later)          */
    int size;          /* object size in bytes (0 for void/func)        */
    int align;         /* alignment in bytes                            */

    Type *base;        /* TY_PTR / TY_ARRAY: pointee / element type     */
    int count;         /* TY_ARRAY: element count (constant)            */

    Type *ret;         /* TY_FUNC: return type                          */
    Type **params;     /* TY_FUNC: parameter types                      */
    int nparams;       /* TY_FUNC: parameter count                      */
    int prototyped;    /* TY_FUNC: 0 = bare () unspecified args          */
};

/* Builtin singleton constructors. */
Type *type_void(void);
Type *type_char(void);
Type *type_int(void);
Type *type_long(void);

/* Derived-type constructors (arena allocated). */
Type *type_ptr(Type *base);
Type *type_array(Type *elem, int count);
Type *type_func(Type *ret, Type **params, int nparams, int prototyped);

/* Predicates. */
int type_is_array(Type *ty);
int type_is_void(Type *ty);
int type_is_char(Type *ty);
int type_is_long(Type *ty);
int type_is_integer(Type *ty);
int type_is_signed(Type *ty);
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
int type_int_width(Type *ty);
int type_int_rank(Type *ty);
Type *type_int_promote(Type *ty);
Type *type_arith_convert(Type *a, Type *b);
Type *type_decay(Type *ty);
Type *type_array_elem(Type *ty);
int type_array_count(Type *ty);
Type *type_ptr_elem(Type *ty);
const char *type_name(Type *ty);
const char *type_func_sig(Type *ty);

#endif /* XCC_TYPE_H */
