/* SPDX-License-Identifier: MIT */
#ifndef XCC_CPP_H
#define XCC_CPP_H

#include <stddef.h>
#include <stdio.h>

#include "source.h"
#include "token.h"

typedef enum {
    CPP_EOF,
    CPP_NEWLINE,
    CPP_IDENT,
    CPP_NUMBER,
    CPP_CHAR,
    CPP_STRING,
    CPP_PUNCT
} CppTokenKind;

typedef struct CppHide CppHide;

typedef struct {
    CppTokenKind kind;
    const char *text;
    size_t len;
    SourceLoc loc;
    int leading_space;
    int starts_line;
    CppHide *hide;
} CppToken;

typedef struct Cpp Cpp;

typedef enum {
    CPP_ACTION_DEFINE,
    CPP_ACTION_UNDEF
} CppActionKind;

typedef struct {
    CppActionKind kind;
    const char *operand;
} CppAction;

typedef struct {
    const char **quote_dirs;
    size_t quote_dir_count;
    const char **include_dirs;
    size_t include_dir_count;
    const char **system_dirs;
    size_t system_dir_count;
    const char *resource_dir;
    const CppAction *actions;
    size_t action_count;
} CppOptions;

Cpp *cpp_create(const SourceFile *source, const CppOptions *options);
CppToken cpp_next(Cpp *cpp);
int cpp_emit(Cpp *cpp, FILE *out);

#endif /* XCC_CPP_H */
