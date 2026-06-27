#ifndef XCC_TOKEN_H
#define XCC_TOKEN_H

/* Shared source position. Owned here because the lexer, parser, AST and
 * diagnostics all need it, and it must flow from the lexer outward. */
typedef struct {
    int line;
    int col;
} SourceLoc;

/* Name of the current input file, used by diagnostics. Defined in main.c. */
extern const char *g_filename;

#endif /* XCC_TOKEN_H */
