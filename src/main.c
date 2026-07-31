/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "diag.h"
#include "lexer.h"
#include "source.h"
#include "sema.h"
#include "sema_typedef.h"
#include "sema_struct.h"
#include "sema_enum.h"
#include "ast_opt.h"
#include "codegen.h"
#include "arena.h"
#include "lower.h"
#include "lir.h"
#include "lir_cfg.h"
#include "lir_opt.h"
#include "liveness.h"
#include "regalloc.h"
#include "target.h"

int yyparse(void);
extern ExternalDecl *g_program;

static void usage(FILE *f)
{
    fprintf(f,
        "xcc 0.0.1 - a C89 compiler (x86-64 AT&T assembly)\n"
        "usage: xcc [file] [-o out] [options]\n"
        "  file        C source file, or - / omitted for stdin\n"
        "  -o out      output file, or - / omitted for stdout\n"
        "  -E          preprocess only; emit normalized, marker-free C\n"
        "  -I dir      add dir to the header search path\n"
        "  -D name[=value]  define a preprocessing macro\n"
        "  -U name     undefine a preprocessing macro\n"
        "  --help      show this help\n"
        "  --help-warnings  list warnings and -W flags\n"
        "  --version   show version\n"
        "  --xcc-dump-raw-lir  dump unoptimized SSA LIR (debug)\n"
        "  --xcc-dump-lir  dump optimized, phi-lowered LIR (debug)\n"
        "  --xcc-dump-lir-alloc  dump LIR liveness and allocation (debug)\n"
        "  --xcc-verify-lir  verify internal LIR (default)\n"
        "  --xcc-no-verify-lir  disable internal LIR verification\n"
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

static void append_cpp_action(CppAction **actions, size_t *count, size_t *cap,
                              CppActionKind kind, const char *operand)
{
    if (*count == *cap) {
        size_t new_cap = *cap ? *cap * 2 : 4;
        CppAction *new_actions = realloc(*actions,
                                         new_cap * sizeof(*new_actions));

        if (!new_actions)
            diag_fatal("out of memory recording command-line macros");
        *actions = new_actions;
        *cap = new_cap;
    }
    (*actions)[*count].kind = kind;
    (*actions)[*count].operand = operand;
    (*count)++;
}

int main(int argc, char **argv)
{
    const char *inpath = NULL;
    const char *outpath = NULL;
    int lir_dump_mode = 0;
    int verify_lir = 1;
    int preprocess_only = 0;
    FILE *in = stdin;
    SourceFile *source;
    const char **include_dirs = NULL;
    size_t include_dir_count = 0;
    size_t include_dir_cap = 0;
    CppAction *actions = NULL;
    size_t action_count = 0;
    size_t action_cap = 0;

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
        } else if (strcmp(argv[i], "-E") == 0) {
            preprocess_only = 1;
        } else if (strcmp(argv[i], "--xcc-dump-raw-lir") == 0) {
            lir_dump_mode = 1;
        } else if (strcmp(argv[i], "--xcc-dump-lir") == 0) {
            lir_dump_mode = 2;
        } else if (strcmp(argv[i], "--xcc-dump-lir-alloc") == 0) {
            lir_dump_mode = 3;
        } else if (strcmp(argv[i], "--xcc-verify-lir") == 0) {
            verify_lir = 1;
        } else if (strcmp(argv[i], "--xcc-no-verify-lir") == 0) {
            verify_lir = 0;
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
        } else if (strncmp(argv[i], "-I", 2) == 0) {
            const char *dir = argv[i] + 2;

            if (!*dir) {
                if (i + 1 >= argc) {
                    diag_error("-I requires an argument");
                    free(include_dirs);
                    return 1;
                }
                dir = argv[++i];
            }
            if (include_dir_count == include_dir_cap) {
                size_t new_cap = include_dir_cap ? include_dir_cap * 2 : 4;
                const char **new_dirs = realloc(include_dirs,
                    new_cap * sizeof(*new_dirs));

                if (!new_dirs)
                    diag_fatal("out of memory recording include paths");
                include_dirs = new_dirs;
                include_dir_cap = new_cap;
            }
            include_dirs[include_dir_count++] = dir;
        } else if (strncmp(argv[i], "-D", 2) == 0 ||
                   strncmp(argv[i], "-U", 2) == 0) {
            CppActionKind kind = argv[i][1] == 'D'
                               ? CPP_ACTION_DEFINE : CPP_ACTION_UNDEF;
            const char *operand = argv[i] + 2;

            if (!*operand) {
                if (i + 1 >= argc) {
                    diag_error("-%c requires an argument", argv[i][1]);
                    free(actions);
                    free(include_dirs);
                    return 1;
                }
                operand = argv[++i];
            }
            append_cpp_action(&actions, &action_count, &action_cap,
                              kind, operand);
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
        in = fopen(inpath, "rb");
        if (!in) {
            diag_error("cannot open '%s'", inpath);
            return 1;
        }
    }
    source = source_read(in, inpath && strcmp(inpath, "-") != 0
                         ? inpath : "<stdin>");
    if (in != stdin)
        fclose(in);
    {
        CppOptions cpp_options = {
            .include_dirs = include_dirs,
            .include_dir_count = include_dir_count,
            .actions = actions,
            .action_count = action_count,
        };

        if (preprocess_only) {
            FILE *out = stdout;
            int ok;

            if (lir_dump_mode != 0) {
                diag_error("-E cannot be combined with LIR dump options");
                free(actions);
                free(include_dirs);
                return 1;
            }
            if (outpath && strcmp(outpath, "-") != 0) {
                out = fopen(outpath, "w");
                if (!out) {
                    diag_error("cannot open '%s' for writing", outpath);
                    free(actions);
                    free(include_dirs);
                    return 1;
                }
            }
            ok = cpp_emit(cpp_create(source, &cpp_options), out);
            if (out != stdout && fclose(out) != 0) {
                diag_error("failed closing '%s'", outpath);
                ok = 0;
            }
            free(actions);
            free(include_dirs);
            arena_free_all();
            return ok && diag_error_count == 0 ? 0 : 1;
        }
        lexer_set_source(source, &cpp_options);
    }
    free(actions);
    free(include_dirs);

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

    ast_optimize_program(external_functions(g_program));

    if (lir_dump_mode != 0) {
        for (Function *fn = external_functions(g_program); fn; fn = fn->next) {
            if (!fn->is_definition)
                continue;
            LirFn *lf = lower_function(fn);
            lir_cfg_rebuild_preds(lf);
            if (verify_lir)
                lir_cfg_verify(lf);
            if (lir_dump_mode != 1) {
                lir_optimize_ssa_function(lf);
                if (verify_lir)
                    lir_cfg_verify(lf);
                lir_cfg_lower(lf);
                if (verify_lir)
                    lir_cfg_verify(lf);
                lir_optimize_function(lf);
                if (verify_lir)
                    lir_cfg_verify(lf);
            }
            if (lir_dump_mode == 3) {
                Liveness lv;
                AllocResult alloc;
                liveness_compute(lf, &X86_SYSV, &lv);
                regalloc_linear(lf, fn, &lv, &X86_SYSV, &alloc);
                if (verify_lir)
                    regalloc_verify(lf, &lv, &X86_SYSV, &alloc);
                /* Splitting materializes fragment vregs and moves, so dump the
                   final LIR and its recomputed liveness rather than the stale
                   pre-allocation form. */
                lir_dump_fn(lf, stdout);
                liveness_dump(lf, &lv, &X86_SYSV, stdout);
                regalloc_dump(lf, &alloc, &X86_SYSV, stdout);
            } else {
                lir_dump_fn(lf, stdout);
            }
            fputc('\n', stdout);
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

    codegen(g_program, out, verify_lir);

    if (out != stdout)
        fclose(out);
    arena_free_all();
    return 0;
}
