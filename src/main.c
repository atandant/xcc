/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"
#include "ast.h"
#include "diag.h"
#include "sema.h"
#include "codegen.h"
#include "arena.h"

extern FILE *yyin;
int yyparse(void);
extern Function *g_program;

const char *g_filename = "<stdin>";

static void usage(FILE *f)
{
    fprintf(f,
        "xcc 0.0.1 - a C89 compiler (x86-64 AT&T assembly)\n"
        "usage: xcc [file] [-o out]\n"
        "  file        C source file, or - / omitted for stdin\n"
        "  -o out      output assembly file, or - / omitted for stdout\n"
        "  --help      show this help\n"
        "  --version   show version\n");
}

int main(int argc, char **argv)
{
    const char *inpath = NULL;
    const char *outpath = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(stdout);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("xcc 0.0.1\n");
            return 0;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "xcc: -o requires an argument\n");
                return 1;
            }
            outpath = argv[++i];
        } else if (argv[i][0] == '-' && argv[i][1] != '\0' &&
                   strcmp(argv[i], "-") != 0) {
            fprintf(stderr, "xcc: unknown option '%s'\n", argv[i]);
            return 1;
        } else {
            if (inpath) {
                fprintf(stderr, "xcc: multiple input files not supported\n");
                return 1;
            }
            inpath = argv[i];
        }
    }

    if (inpath && strcmp(inpath, "-") != 0) {
        yyin = fopen(inpath, "r");
        if (!yyin) {
            fprintf(stderr, "xcc: cannot open '%s'\n", inpath);
            return 1;
        }
        g_filename = inpath;
    } else {
        yyin = stdin;
        g_filename = "<stdin>";
    }

    if (yyparse() != 0 || diag_error_count > 0)
        return 1;

    if (!g_program) {
        fprintf(stderr, "xcc: empty translation unit\n");
        return 1;
    }

    sema(g_program);
    if (diag_error_count > 0)
        return 1;

    FILE *out = stdout;
    if (outpath && strcmp(outpath, "-") != 0) {
        out = fopen(outpath, "w");
        if (!out) {
            fprintf(stderr, "xcc: cannot open '%s' for writing\n", outpath);
            return 1;
        }
    }

    codegen(g_program, out);

    if (out != stdout)
        fclose(out);
    arena_free_all();
    return 0;
}
