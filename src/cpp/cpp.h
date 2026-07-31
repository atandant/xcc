/* SPDX-License-Identifier: MIT */
#ifndef XCC_CPP_H
#define XCC_CPP_H

#include <stddef.h>

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

typedef struct {
    CppTokenKind kind;
    const char *text;
    size_t len;
    SourceLoc loc;
    int leading_space;
    int starts_line;
} CppToken;

typedef struct Cpp Cpp;

Cpp *cpp_create(const SourceFile *source);
CppToken cpp_next(Cpp *cpp);

#endif /* XCC_CPP_H */
