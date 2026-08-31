/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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
void parser_reset(void);
extern ExternalDecl *g_program;

static void usage(FILE *f)
{
    fprintf(f,
        "xcc - near-stable C89 compiler for x86-64\n"
        "usage: xcc [options] file...\n"
        "  file        C source file, or - for stdin\n"
        "  -o out      executable, object, assembly, or preprocessed output\n"
        "  -E          preprocess only; emit normalized, marker-free C\n"
        "  -S          compile to assembly without assembling\n"
        "  -c          compile to an object without linking\n"
        "  -I dir      add dir to the header search path\n"
        "  -iquote dir search dir for quoted includes only\n"
        "  -isystem dir  add dir as a system header search path\n"
        "  -nostdinc   do not search xcc's bundled headers\n"
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

static void append_include_dir(const char ***dirs, size_t *count, size_t *cap,
                               const char *dir)
{
    if (*count == *cap) {
        size_t new_cap = *cap ? *cap * 2 : 4;
        const char **new_dirs = realloc(*dirs, new_cap * sizeof(*new_dirs));

        if (!new_dirs)
            diag_fatal("out of memory recording include paths");
        *dirs = new_dirs;
        *cap = new_cap;
    }
    (*dirs)[(*count)++] = dir;
}

static const char *resource_include_dir(void)
{
    static char path[4096];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    char *slash;

    if (len <= 0 || (size_t)len >= sizeof(path) - sizeof("/include"))
        return NULL;
    path[len] = '\0';
    slash = strrchr(path, '/');
    if (!slash)
        return NULL;
    if ((size_t)(slash - path) + sizeof("/include") > sizeof(path))
        return NULL;
    memcpy(slash, "/include", sizeof("/include"));
    return path;
}

static void append_input(const char ***inputs, size_t *count, size_t *cap,
                         const char *path)
{
    append_include_dir(inputs, count, cap, path);
}

static void reset_translation_unit(void)
{
    parser_reset();
    ast_reset();
    typedef_reset();
    struct_tag_reset();
    enum_reset();
}

static int compile_one(const char *inpath, const char *outpath,
                       const CppOptions *cpp_options, int preprocess_only,
                       int lir_dump_mode, int verify_lir)
{
    FILE *in = stdin;
    FILE *out = stdout;
    SourceFile *source;
    int errors_before = diag_error_count;
    int ok = 0;

    reset_translation_unit();
    if (inpath && strcmp(inpath, "-") != 0) {
        in = fopen(inpath, "rb");
        if (!in) {
            diag_error("cannot open '%s'", inpath);
            goto done;
        }
    }
    source = source_read(in, inpath && strcmp(inpath, "-") != 0
                         ? inpath : "<stdin>");
    if (in != stdin) {
        fclose(in);
        in = stdin;
    }
    if (outpath && strcmp(outpath, "-") != 0) {
        out = fopen(outpath, "w");
        if (!out) {
            diag_error("cannot open '%s' for writing", outpath);
            goto done;
        }
    }
    if (preprocess_only) {
        ok = cpp_emit(cpp_create(source, cpp_options), out);
        goto done;
    }

    lexer_set_source(source, cpp_options);
    if (yyparse() != 0 || diag_error_count > errors_before)
        goto done;
    if (!g_program) {
        diag_error("empty translation unit");
        goto done;
    }
    sema(g_program);
    if (diag_error_count > errors_before)
        goto done;
    ast_optimize_program(external_functions(g_program));

    if (lir_dump_mode != 0) {
        for (Function *fn = external_functions(g_program); fn; fn = fn->next) {
            LirFn *lf;

            if (!fn->is_definition)
                continue;
            lf = lower_function(fn);
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
                lir_dump_fn(lf, stdout);
                liveness_dump(lf, &lv, &X86_SYSV, stdout);
                regalloc_dump(lf, &alloc, &X86_SYSV, stdout);
            } else {
                lir_dump_fn(lf, stdout);
            }
            fputc('\n', stdout);
        }
    } else {
        codegen(g_program, out, verify_lir);
    }
    ok = 1;

done:
    if (in != stdin)
        fclose(in);
    if (out != stdout && fclose(out) != 0) {
        diag_error("failed closing '%s'", outpath);
        ok = 0;
    }
    reset_translation_unit();
    arena_free_all();
    return ok && diag_error_count == errors_before;
}

static char *default_output_name(const char *input, const char *suffix)
{
    const char *base = strrchr(input, '/');
    const char *dot;
    size_t stem_len;
    char *name;

    base = base ? base + 1 : input;
    dot = strrchr(base, '.');
    stem_len = dot && dot != base ? (size_t)(dot - base) : strlen(base);
    name = malloc(stem_len + strlen(suffix) + 1);
    if (!name)
        diag_fatal("out of memory creating output name");
    memcpy(name, base, stem_len);
    strcpy(name + stem_len, suffix);
    return name;
}

static int run_command(char *const command[])
{
    pid_t child = fork();
    int status;

    if (child < 0) {
        diag_error("cannot start '%s'", command[0]);
        return 0;
    }
    if (child == 0) {
        execvp(command[0], command);
        fprintf(stderr, "xcc: error: cannot execute '%s': %s\n",
                command[0], strerror(errno));
        _exit(127);
    }
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            diag_error("failed waiting for '%s'", command[0]);
            return 0;
        }
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(int argc, char **argv)
{
    const char *outpath = NULL;
    const char **inputs = NULL;
    size_t input_count = 0;
    size_t input_cap = 0;
    int lir_dump_mode = 0;
    int verify_lir = 1;
    int preprocess_only = 0;
    int assembly_only = 0;
    int compile_only = 0;
    int no_stdinc = 0;
    const char **quote_dirs = NULL;
    size_t quote_dir_count = 0;
    size_t quote_dir_cap = 0;
    const char **include_dirs = NULL;
    size_t include_dir_count = 0;
    size_t include_dir_cap = 0;
    const char **system_dirs = NULL;
    size_t system_dir_count = 0;
    size_t system_dir_cap = 0;
    const char *resource_dir = NULL;
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
            printf("xcc (near-stable)\n");
            return 0;
        } else if (strcmp(argv[i], "-E") == 0) {
            preprocess_only = 1;
        } else if (strcmp(argv[i], "-S") == 0) {
            assembly_only = 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            compile_only = 1;
        } else if (strcmp(argv[i], "-nostdinc") == 0) {
            no_stdinc = 1;
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
        } else if (strncmp(argv[i], "-iquote", 7) == 0) {
            const char *dir = argv[i] + 7;

            if (!*dir) {
                if (i + 1 >= argc) {
                    diag_error("-iquote requires an argument");
                    return 1;
                }
                dir = argv[++i];
            }
            append_include_dir(&quote_dirs, &quote_dir_count,
                               &quote_dir_cap, dir);
        } else if (strncmp(argv[i], "-isystem", 8) == 0) {
            const char *dir = argv[i] + 8;

            if (!*dir) {
                if (i + 1 >= argc) {
                    diag_error("-isystem requires an argument");
                    return 1;
                }
                dir = argv[++i];
            }
            append_include_dir(&system_dirs, &system_dir_count,
                               &system_dir_cap, dir);
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
            append_include_dir(&include_dirs, &include_dir_count,
                               &include_dir_cap, dir);
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
            append_input(&inputs, &input_count, &input_cap, argv[i]);
        }
    }

    if (!no_stdinc)
        resource_dir = resource_include_dir();
    {
        CppOptions cpp_options = {
            .quote_dirs = quote_dirs,
            .quote_dir_count = quote_dir_count,
            .include_dirs = include_dirs,
            .include_dir_count = include_dir_count,
            .system_dirs = system_dirs,
            .system_dir_count = system_dir_count,
            .resource_dir = resource_dir,
            .actions = actions,
            .action_count = action_count,
        };
        int modes = preprocess_only + assembly_only + compile_only;
        int status = 0;

        if (modes > 1) {
            diag_error("-E, -S, and -c are mutually exclusive");
            status = 1;
            goto finish;
        }
        if (lir_dump_mode && (preprocess_only || compile_only)) {
            diag_error("LIR dump options require assembly compilation");
            status = 1;
            goto finish;
        }
        if (input_count == 0)
            append_input(&inputs, &input_count, &input_cap, "-");
        if ((preprocess_only || lir_dump_mode) && input_count != 1) {
            diag_error("this mode requires exactly one input file");
            status = 1;
            goto finish;
        }
        if (outpath && input_count > 1 && (assembly_only || compile_only)) {
            diag_error("cannot use -o with multiple inputs under -S or -c");
            status = 1;
            goto finish;
        }
        if (preprocess_only || lir_dump_mode) {
            status = !compile_one(inputs[0], outpath, &cpp_options,
                                  preprocess_only, lir_dump_mode, verify_lir);
            goto finish;
        }
        if (assembly_only) {
            for (size_t i = 0; i < input_count; i++) {
                char *name = NULL;
                const char *output = outpath;

                if (!output && strcmp(inputs[i], "-") != 0) {
                    name = default_output_name(inputs[i], ".s");
                    output = name;
                }
                if (!compile_one(inputs[i], output, &cpp_options, 0, 0,
                                 verify_lir))
                    status = 1;
                free(name);
                if (status)
                    break;
            }
            goto finish;
        }
        {
            const char *cc = getenv("CC");
            char temp_template[] = "/tmp/xcc-XXXXXX";
            char *temp_dir;
            char **objects = calloc(input_count, sizeof(*objects));

            if (!cc || !*cc)
                cc = "cc";
            temp_dir = mkdtemp(temp_template);
            if (!temp_dir || !objects) {
                diag_error("cannot create temporary compilation directory");
                free(objects);
                status = 1;
                goto finish;
            }
            for (size_t i = 0; i < input_count; i++) {
                char *assembly = malloc(strlen(temp_dir) + 32);
                char *object;
                char *default_object = NULL;
                char *command[6];

                if (!assembly) {
                    status = 1;
                    break;
                }
                sprintf(assembly, "%s/unit%zu.s", temp_dir, i);
                if (compile_only) {
                    if (outpath)
                        object = (char *)outpath;
                    else if (strcmp(inputs[i], "-") != 0)
                        object = default_object =
                            default_output_name(inputs[i], ".o");
                    else
                        object = "-.o";
                } else {
                    object = malloc(strlen(temp_dir) + 32);
                    if (object)
                        sprintf(object, "%s/unit%zu.o", temp_dir, i);
                }
                if (!object || !compile_one(inputs[i], assembly, &cpp_options,
                                            0, 0, verify_lir)) {
                    free(assembly);
                    free(default_object);
                    if (!compile_only)
                        free(object);
                    status = 1;
                    break;
                }
                command[0] = (char *)cc;
                command[1] = "-c";
                command[2] = assembly;
                command[3] = "-o";
                command[4] = object;
                command[5] = NULL;
                if (!run_command(command))
                    status = 1;
                unlink(assembly);
                free(assembly);
                if (compile_only) {
                    free(default_object);
                } else {
                    objects[i] = object;
                }
                if (status)
                    break;
            }
            if (!status && !compile_only) {
                char **command = calloc(input_count + 4, sizeof(*command));

                if (!command) {
                    status = 1;
                } else {
                    command[0] = (char *)cc;
                    for (size_t i = 0; i < input_count; i++)
                        command[i + 1] = objects[i];
                    command[input_count + 1] = "-o";
                    command[input_count + 2] =
                        (char *)(outpath ? outpath : "a.out");
                    if (!run_command(command))
                        status = 1;
                    free(command);
                }
            }
            for (size_t i = 0; i < input_count; i++) {
                if (objects[i]) {
                    unlink(objects[i]);
                    free(objects[i]);
                }
            }
            free(objects);
            rmdir(temp_dir);
        }

finish:
        free(inputs);
        free(actions);
        free(quote_dirs);
        free(include_dirs);
        free(system_dirs);
        return status;
    }
}
