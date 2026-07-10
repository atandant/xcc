/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"
#include "ast.h"
#include "diag.h"
#include "sema.h"
#include "sema_typedef.h"
#include "sema_struct.h"
#include "sema_enum.h"
#include "copts.h"
#include "codegen.h"
#include "arena.h"
#include "lower.h"
#include "lir.h"
#include "liveness.h"
#include "target.h"

extern FILE *yyin;
int yyparse(void);
extern Function *g_program;

const char *g_filename = "<stdin>";

static void usage(FILE *f)
{
    fprintf(f,
        "xcc 0.0.1 - a C89 compiler (x86-64 AT&T assembly)\n"
        "usage: xcc [file] [-o out] [options]\n"
        "  file        C source file, or - / omitted for stdin\n"
        "  -o out      output assembly file, or - / omitted for stdout\n"
        "  --help      show this help\n"
        "  --help-warnings  list warnings and -W flags\n"
        "  --version   show version\n"
        "  --xcc-dump-lir  dump lowered LIR to stdout (debug)\n"
        "  --xcc-dump-lir-alloc  dump LIR live intervals (debug)\n"
        "  -W<name>    enable warning (see --help-warnings)\n"
        "  -Wno-<name> disable warning\n"
        "  -Wall       enable warnings that default to off\n"
        "  -w          disable all warnings\n"
        "  -Werror     treat warnings as errors\n");
}

static void unknown_warn_flag(const char *arg)
{
    diag_error("unknown warning option '%s'", arg);
    fputs("xcc: known warnings:", stderr);
    for (int i = 0; i < W_COUNT; i++)
        fprintf(stderr, " %s", diag_warn_name(i));
    fputc('\n', stderr);
}

static char **load_source_lines(FILE *f, int *out_nlines)
{
    char **lines = NULL;
    int n = 0;
    int cap = 0;
    char buf[4096];

    rewind(f);
    while (fgets(buf, sizeof buf, f)) {
        size_t len = strlen(buf);

        if (n >= cap) {
            cap = cap ? cap * 2 : 64;
            lines = realloc(lines, (size_t)cap * sizeof(*lines));
            if (!lines)
                diag_fatal("out of memory reading source");
        }
        if (len > 0 && buf[len - 1] == '\n')
            buf[len - 1] = '\0';
        lines[n++] = arena_strdup(buf);
    }
    *out_nlines = n;
    return lines;
}

int main(int argc, char **argv)
{
    const char *inpath = NULL;
    const char *outpath = NULL;
    int emit_lir = 0;
    int emit_lir_alloc = 0;
    char **source_lines = NULL;
    int nsource_lines = 0;
    int from_file = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage(stdout);
            return 0;
        } else if (strcmp(argv[i], "--help-warnings") == 0) {
            diag_print_warnings_help(stdout);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("xcc 0.0.1\n");
            return 0;
        } else if (strcmp(argv[i], "--xcc-dump-lir") == 0) {
            emit_lir = 1;
        } else if (strcmp(argv[i], "--xcc-dump-lir-alloc") == 0) {
            emit_lir_alloc = 1;
        } else if (strcmp(argv[i], "-w") == 0) {
            diag_disable_all_warnings();
        } else if (strncmp(argv[i], "-W", 2) == 0) {
            int r = diag_apply_warn_flag(argv[i]);

            if (r == 1)
                unknown_warn_flag(argv[i]);
            else if (r != 0)
                diag_error("malformed warning option '%s'", argv[i]);
            if (r != 0)
                return 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                diag_error("-o requires an argument");
                return 1;
            }
            outpath = argv[++i];
        } else if (argv[i][0] == '-' && argv[i][1] != '\0' &&
                   strcmp(argv[i], "-") != 0) {
            diag_error("unknown option '%s'", argv[i]);
            return 1;
        } else {
            if (inpath) {
                diag_error("multiple input files not supported");
                return 1;
            }
            inpath = argv[i];
        }
    }

    if (inpath && strcmp(inpath, "-") != 0) {
        yyin = fopen(inpath, "r");
        if (!yyin) {
            diag_error("cannot open '%s'", inpath);
            return 1;
        }
        g_filename = inpath;
        from_file = 1;
        source_lines = load_source_lines(yyin, &nsource_lines);
        diag_set_source(source_lines, nsource_lines);
        rewind(yyin);
    } else {
        yyin = stdin;
        g_filename = "<stdin>";
    }

    typedef_reset();
    struct_tag_reset();
    enum_reset();

    if (yyparse() != 0 || diag_error_count > 0)
        return 1;

    if (!g_program) {
        diag_error("empty translation unit");
        return 1;
    }

    sema(g_program);
    if (diag_error_count > 0)
        return 1;

    copts_optimize(g_program);

    if (emit_lir || emit_lir_alloc) {
        for (Function *fn = g_program; fn; fn = fn->next) {
            if (!fn->is_definition)
                continue;
            LirFn *lf = lower_function(fn);
            if (emit_lir_alloc) {
                Liveness lv;
                liveness_compute(lf, &X86_SYSV, &lv);
                liveness_dump(lf, &lv, &X86_SYSV, stdout);
            } else {
                lir_dump_fn(lf, stdout);
            }
            fputc('\n', stdout);
        }
        if (from_file) {
            free(source_lines);
            fclose(yyin);
        }
        arena_free_all();
        return 0;
    }

    FILE *out = stdout;
    if (outpath && strcmp(outpath, "-") != 0) {
        out = fopen(outpath, "w");
        if (!out) {
            diag_error("cannot open '%s' for writing", outpath);
            return 1;
        }
    }

    codegen(g_program, out);

    if (out != stdout)
        fclose(out);
    if (from_file) {
        free(source_lines);
        fclose(yyin);
    }
    arena_free_all();
    return 0;
}
