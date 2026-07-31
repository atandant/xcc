/* SPDX-License-Identifier: MIT */
#ifndef XCC_TOKEN_H
#define XCC_TOKEN_H

typedef struct SourceFile SourceFile;

/* Shared source position. Owned here because the lexer, parser, AST and
 * diagnostics all need it, and it must flow from the lexer outward. */
typedef struct {
    const SourceFile *file;
    int line;
    int col;
} SourceLoc;

/* Bison locations preserve source identity in addition to the usual range. */
typedef struct {
    const SourceFile *file;
    int first_line;
    int first_column;
    int last_line;
    int last_column;
} XccLocation;

#endif /* XCC_TOKEN_H */
