/* SPDX-License-Identifier: MIT */
#include "diag.h"
#include "source.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int diag_error_count = 0;
int diag_warning_count = 0;

/* GCC-like ANSI styling; disabled when stderr is not a tty or NO_COLOR is set. */
static const char *sgr_reset = "";
static const char *sgr_bold = "";
static const char *sgr_error = "";
static const char *sgr_warning = "";
static const char *sgr_note = "";
static int colors_inited;

typedef struct {
    const char *name;
    int default_enabled;
    int enabled;
    int as_error;
    int error_suppress; /* -Wno-error=<name> under global -Werror */
    int in_wall;        /* enabled by -Wall */
} WarnOpt;

static int warn_error_global;

static WarnOpt warn_opts[W_COUNT] = {
    [W_IMPLICIT_FUNCTION_DECLARATION] = {
        "implicit-function-declaration", 1, 1, 0, 0, 0,
    },
    [W_CALL_WITHOUT_PROTOTYPE] = {
        "call-without-prototype", 1, 1, 0, 0, 0,
    },
    [W_CHAR_CONSTANT_OVERFLOW] = {
        "char-constant-overflow", 1, 1, 0, 0, 0,
    },
    [W_CHAR_VALUE_NARROWING] = {
        "char-value-narrowing", 0, 0, 0, 0, 1,
    },
    [W_RETURN_TYPE] = {
        "return-type", 1, 1, 0, 0, 0,
    },
    [W_OLD_STYLE_DEFINITION] = {
        "old-style-definition", 1, 1, 0, 0, 0,
    },
    [W_IMPLICIT_VOID_POINTER] = {
        "implicit-void-pointer", 0, 0, 0, 0, 1,
    },
    [W_SELF_REFERENTIAL_INITIALIZER] = {
        "self-referential-initializer", 1, 1, 0, 0, 0,
    },
    [W_PRAGMAS] = {
        "pragmas", 1, 1, 0, 0, 0,
    },
};

static void diag_init_colors(void)
{
    const char *no_color = getenv("NO_COLOR");

    if (no_color && no_color[0] != '\0') {
        sgr_reset = sgr_bold = sgr_error = sgr_warning = sgr_note = "";
        return;
    }
    if (!isatty(STDERR_FILENO)) {
        sgr_reset = sgr_bold = sgr_error = sgr_warning = sgr_note = "";
        return;
    }
    sgr_reset = "\033[0m";
    sgr_bold = "\033[1m";
    sgr_error = "\033[1;31m";
    sgr_warning = "\033[1;33m";
    sgr_note = "\033[1;36m";
}

static void diag_ensure_colors(void)
{
    if (!colors_inited) {
        diag_init_colors();
        colors_inited = 1;
    }
}

static void diag_emit_caret(SourceLoc loc)
{
    const char *line = source_line(loc.file, loc.line);
    size_t line_len;

    if (!line)
        return;
    line_len = strlen(line);
    fputs(line, stderr);
    fputc('\n', stderr);

    for (int i = 1; i < loc.col; i++) {
        char c = (size_t)(i - 1) < line_len ? line[i - 1] : ' ';
        fputc(c == '\t' ? '\t' : ' ', stderr);
    }
    fputc('^', stderr);
    fputc('\n', stderr);
}

static void diag_emit_include_chain(const SourceFile *source)
{
    SourceLoc origin;

    if (!source_include_origin(source, &origin))
        return;
    diag_emit_include_chain(origin.file);
    fprintf(stderr, "In file included from %s:%d:%d:\n",
            source_name(origin.file), origin.line, origin.col);
}

static void diag_vemit_at(SourceLoc loc, const char *kind, const char *sgr_kind,
                          const char *fmt, va_list ap)
{
    diag_ensure_colors();
    diag_emit_include_chain(loc.file);

    fprintf(stderr, "%s%s:%d:%d:%s ", sgr_bold, source_name(loc.file),
            loc.line, loc.col, sgr_reset);
    fprintf(stderr, "%s%s:%s ", sgr_kind, kind, sgr_reset);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    diag_emit_caret(loc);
}

static void diag_vwarning_at(SourceLoc loc, const char *fmt, va_list ap)
{
    diag_vemit_at(loc, "warning", sgr_warning, fmt, ap);
    diag_warning_count++;
}

static void diag_verror_at(SourceLoc loc, const char *fmt, va_list ap)
{
    diag_vemit_at(loc, "error", sgr_error, fmt, ap);
    diag_error_count++;
}

void diag_error_at(SourceLoc loc, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    diag_verror_at(loc, fmt, ap);
    va_end(ap);
}

void diag_warning_at(SourceLoc loc, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    diag_vwarning_at(loc, fmt, ap);
    va_end(ap);
}

void diag_note_at(SourceLoc loc, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    diag_vemit_at(loc, "note", sgr_note, fmt, ap);
    va_end(ap);
}

int diag_warn_enabled(DiagWarnId id)
{
    if (id < 0 || id >= W_COUNT)
        return 0;
    return warn_opts[id].enabled;
}

int diag_warn_as_error(DiagWarnId id)
{
    if (id < 0 || id >= W_COUNT)
        return 0;
    if (warn_opts[id].error_suppress)
        return 0;
    return warn_error_global || warn_opts[id].as_error;
}

int diag_warn_default_enabled(DiagWarnId id)
{
    if (id < 0 || id >= W_COUNT)
        return 0;
    return warn_opts[id].default_enabled;
}

int diag_warn_in_wall_group(DiagWarnId id)
{
    if (id < 0 || id >= W_COUNT)
        return 0;
    return warn_opts[id].in_wall;
}

const char *diag_warn_name(DiagWarnId id)
{
    if (id < 0 || id >= W_COUNT)
        return "";
    return warn_opts[id].name;
}

void diag_set_warn_enabled(DiagWarnId id, int enabled)
{
    if (id < 0 || id >= W_COUNT)
        return;
    warn_opts[id].enabled = enabled ? 1 : 0;
}

void diag_set_warn_as_error(DiagWarnId id, int as_error)
{
    if (id < 0 || id >= W_COUNT)
        return;
    warn_opts[id].as_error = as_error ? 1 : 0;
    if (as_error)
        warn_opts[id].error_suppress = 0;
}

void diag_set_warn_error_suppress(DiagWarnId id, int suppress)
{
    if (id < 0 || id >= W_COUNT)
        return;
    warn_opts[id].error_suppress = suppress ? 1 : 0;
    if (suppress)
        warn_opts[id].as_error = 0;
}

static int diag_warn_id_by_name(const char *name)
{
    if (!name || !name[0])
        return -1;

    for (int i = 0; i < W_COUNT; i++) {
        if (strcmp(warn_opts[i].name, name) == 0)
            return i;
    }
    return -1;
}

void diag_disable_all_warnings(void)
{
    for (int i = 0; i < W_COUNT; i++)
        warn_opts[i].enabled = 0;
}

void diag_enable_wall_warnings(void)
{
    for (int i = 0; i < W_COUNT; i++) {
        if (warn_opts[i].in_wall)
            warn_opts[i].enabled = 1;
    }
}

static int diag_apply_named_warn(const char *name, int enable)
{
    int id = diag_warn_id_by_name(name);

    if (id < 0)
        return 1;
    diag_set_warn_enabled(id, enable);
    return 0;
}

static int diag_apply_named_error(const char *name, int as_error)
{
    int id = diag_warn_id_by_name(name);

    if (id < 0)
        return 1;
    if (as_error)
        diag_set_warn_as_error(id, 1);
    else
        diag_set_warn_error_suppress(id, 1);
    return 0;
}

int diag_apply_warn_flag(const char *arg)
{
    const char *name;

    if (!arg || strncmp(arg, "-W", 2) != 0)
        return 2;

    if (strcmp(arg, "-Wall") == 0) {
        diag_enable_wall_warnings();
        return 0;
    }
    if (strcmp(arg, "-Werror") == 0) {
        warn_error_global = 1;
        return 0;
    }
    if (strcmp(arg, "-Wno-error") == 0) {
        warn_error_global = 0;
        for (int i = 0; i < W_COUNT; i++)
            warn_opts[i].error_suppress = 0;
        return 0;
    }

    if (strncmp(arg, "-Wno-error=", 11) == 0)
        return diag_apply_named_error(arg + 11, 0);
    if (strncmp(arg, "-Werror=", 8) == 0)
        return diag_apply_named_error(arg + 8, 1);
    if (strncmp(arg, "-Wno-", 5) == 0)
        return diag_apply_named_warn(arg + 5, 0);
    if (strncmp(arg, "-W", 2) == 0) {
        name = arg + 2;
        if (!name[0])
            return 2;
        return diag_apply_named_warn(name, 1);
    }

    return 2;
}

void diag_print_warnings_help(FILE *f)
{
    fprintf(f, "xcc warnings (use -W<name> / -Wno-<name>):\n\n");
    for (int i = 0; i < W_COUNT; i++) {
        const char *state = warn_opts[i].default_enabled ? "on" : "off";
        const char *wall = warn_opts[i].in_wall ? "  (-Wall)" : "";

        fprintf(f, "  %-32s %s%s\n", warn_opts[i].name, state, wall);
    }
    fputs("\nWarning control:\n", f);
    fputs("  -Wall                 enable warnings that default to off\n", f);
    fputs("  -w                    disable all warnings\n", f);
    fputs("  -Werror               treat warnings as errors\n", f);
    fputs("  -Werror=<name>        treat one warning as an error\n", f);
    fputs("  -Wno-error            do not treat warnings as errors\n", f);
    fputs("  -Wno-error=<name>     exempt one warning from -Werror\n", f);
}

void diag_warn(DiagWarnId id, SourceLoc loc, const char *fmt, ...)
{
    va_list ap;

    if (!diag_warn_enabled(id) || source_is_system_header(loc.file))
        return;

    va_start(ap, fmt);
    if (diag_warn_as_error(id))
        diag_verror_at(loc, fmt, ap);
    else
        diag_vwarning_at(loc, fmt, ap);
    va_end(ap);
}

static void diag_vemit_plain(const char *kind, const char *sgr_kind,
                             const char *fmt, va_list ap)
{
    diag_ensure_colors();

    fputs("xcc: ", stderr);
    fprintf(stderr, "%s%s:%s ", sgr_kind, kind, sgr_reset);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

void diag_error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    diag_vemit_plain("error", sgr_error, fmt, ap);
    va_end(ap);
    diag_error_count++;
}

void diag_fatal(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    diag_vemit_plain("error", sgr_error, fmt, ap);
    va_end(ap);
    exit(1);
}
