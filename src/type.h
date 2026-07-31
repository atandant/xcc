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
    TY_INT,     /* char, short, int, long; signed and unsigned variants */
    TY_PTR,
    TY_ARRAY,
    TY_FUNC,
    TY_TYPEDEF_REF, /* parser placeholder: resolve via typedef name in sema */
    TY_STRUCT,
    TY_UNION,   /* like TY_STRUCT but all members overlap at offset 0    */
    TY_ENUM     /* enumerated type; behaves as `int` in all expressions  */
} TypeKind;

typedef enum {
    IW_CHAR,
    IW_SHORT,
    IW_INT,
    IW_LONG
} IntWidth;

typedef enum {
    IS_SIGNED,
    IS_UNSIGNED
} IntSign;

typedef struct Type Type;

typedef struct Member Member;
struct Member {
    char *name;
    Type *ty;
    int offset;
    int is_bitfield;
    int bit_width;   /* 0 for unnamed `: 0` padding markers */
    int bit_offset;
};

struct Type {
    TypeKind kind;
    IntWidth width;    /* TY_INT: char / short / int / long              */
    IntSign sign;      /* TY_INT: signedness                             */
    int size;          /* object size in bytes (0 for void/func)        */
    int align;         /* alignment in bytes                            */

    Type *base;        /* TY_PTR / TY_ARRAY: pointee / element type     */
    int count;         /* TY_ARRAY: element count (constant)            */

    Type *ret;         /* TY_FUNC: return type                          */
    Type **params;     /* TY_FUNC: parameter type                       */
    int nparams;       /* TY_FUNC: parameter count                      */
    int prototyped;    /* TY_FUNC: 0 = bare () unspecified args          */

    char *ref_name;    /* TY_TYPEDEF_REF: pending typedef identifier     */

    char *tag;         /* TY_STRUCT: struct tag name (arena-owned)       */
    int is_complete;   /* TY_STRUCT: 1 when body has been defined        */
    Member *members;   /* TY_STRUCT: member array (arena-owned)          */
    int nmembers;      /* TY_STRUCT: member count                        */
};

typedef struct {
    int width;      /* bytes: 1, 2, 4, 8 */
    int is_signed;  /* xcc treats plain char as unsigned-byte */
    int rank;       /* C integer conversion rank */
} TypeIntInfo;

typedef struct {
    int width;      /* machine representation width in bytes */
    int is_signed;  /* meaningful for integer scalars */
    int rank;       /* integer rank, or 0 for pointers */
    int is_integer;
    int is_pointer;
} TypeScalarInfo;

/* Builtin singleton constructors. */
Type *type_void(void);
Type *type_char(void);
Type *type_int(void);
Type *type_long(void);
Type *type_short(void);
Type *type_signed_char(void);
Type *type_unsigned_char(void);
Type *type_unsigned_short(void);
Type *type_unsigned_int(void);
Type *type_unsigned_long(void);

/* Derived-type constructors (arena allocated). */
Type *type_ptr(Type *base);
Type *type_array(Type *elem, int count);
Type *type_func(Type *ret, Type **params, int nparams, int prototyped);
Type *type_typedef_ref(char *name);
Type *type_enum(char *tag);
void type_struct_layout(Type *ty);
int type_is_struct(Type *ty);
int type_is_union(Type *ty);
int type_is_record(Type *ty);  /* struct or union */
int type_is_enum(Type *ty);
int type_struct_is_complete(Type *ty);
const char *type_struct_tag(Type *ty);
Member *type_struct_member(Type *ty, const char *name, int *out_index);

/* Predicates. */
int type_is_typedef_ref(Type *ty);
const char *type_typedef_ref_name(Type *ty);
int type_is_array(Type *ty);
int type_is_void(Type *ty);
int type_is_plain_char(Type *ty);   /* plain `char` only */
int type_is_signed_char(Type *ty);  /* `signed char` only */
int type_is_unsigned_char(Type *ty); /* `unsigned char` only */
int type_is_char(Type *ty);         /* any char width */
int type_is_long(Type *ty);         /* any long width */
int type_is_short(Type *ty);        /* any short width */
int type_is_integer(Type *ty);
int type_is_signed(Type *ty);
int type_is_unsigned(Type *ty);
int type_is_pointer(Type *ty);
int type_is_function_pointer(Type *ty);
int type_is_scalar(Type *ty);
int type_is_object(Type *ty);
int type_is_complete(Type *ty);

/* Relations. */
int type_same(Type *a, Type *b);
int type_compatible(Type *a, Type *b);
int type_assignable(Type *dst, Type *src);

/* Queries. */
int type_size(Type *ty);
int type_align(Type *ty);
int type_int_width(Type *ty);
int type_int_rank(Type *ty);
int type_int_info(Type *ty, TypeIntInfo *out);
int type_scalar_info(Type *ty, TypeScalarInfo *out);
/* Applies a C89 integer conversion of value `v` to integer type `ty`
   (truncate mod 2^width, reinterpret with ty's signedness). */
long type_convert_const(long v, Type *ty);
long type_convert_const_from(long v, Type *src, Type *dst);
Type *type_int_promote(Type *ty);
Type *type_arith_convert(Type *a, Type *b);
Type *type_classify_integer_constant(long v, int has_long_suffix,
                                     int has_unsigned_suffix,
                                     int is_nondecimal);
Type *type_classify_hex_constant(unsigned long v);
Type *type_classify_octal_constant(unsigned long v);
Type *type_decay(Type *ty);
Type *type_array_elem(Type *ty);
int type_array_count(Type *ty);
Type *type_ptr_elem(Type *ty);
int type_cast_target_ok(Type *ty);
const char *type_name(Type *ty);
const char *type_func_sig(Type *ty);

#endif /* XCC_TYPE_H */
