/* SPDX-License-Identifier: MIT */
#include "diag.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int diag_error_count = 0;

static char **diag_lines;
static int diag_nlines;

/* GCC-like ANSI styling; disabled when stderr is not a tty or NO_COLOR is set. */
static const char *sgr_reset = "";
static const char *sgr_bold = "";
static const char *sgr_error = "";
static const char *sgr_note = "";
static int colors_inited;

static void diag_init_colors(void)
{
    const char *no_color = getenv("NO_COLOR");

    if (no_color && no_color[0] != '\0') {
        sgr_reset = sgr_bold = sgr_error = sgr_note = "";
        return;
    }
    if (!isatty(STDERR_FILENO)) {
        sgr_reset = sgr_bold = sgr_error = sgr_note = "";
        return;
    }
    sgr_reset = "\033[0m";
    sgr_bold = "\033[1m";
    sgr_error = "\033[1;31m";
    sgr_note = "\033[1;36m";
}

static void diag_ensure_colors(void)
{
    if (!colors_inited) {
        diag_init_colors();
        colors_inited = 1;
    }
}

void diag_set_source(char **lines, int nlines)
{
    diag_ensure_colors();
    diag_lines = lines;
    diag_nlines = nlines;
}

static void diag_emit_caret(SourceLoc loc)
{
    const char *line;

    if (!diag_lines || loc.line < 1 || loc.line > diag_nlines)
        return;

    line = diag_lines[loc.line - 1];
    fputs(line, stderr);
    fputc('\n', stderr);

    for (int i = 1; i < loc.col; i++) {
        char c = line[i - 1];
        fputc(c == '\t' ? '\t' : ' ', stderr);
    }
    fputc('^', stderr);
    fputc('\n', stderr);
}

static void diag_vemit_at(SourceLoc loc, const char *kind, const char *sgr_kind,
                          const char *fmt, va_list ap)
{
    diag_ensure_colors();

    fprintf(stderr, "%s%s:%d:%d:%s ", sgr_bold, g_filename, loc.line, loc.col,
            sgr_reset);
    fprintf(stderr, "%s%s:%s ", sgr_kind, kind, sgr_reset);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    diag_emit_caret(loc);
}

void diag_error_at(SourceLoc loc, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    diag_vemit_at(loc, "error", sgr_error, fmt, ap);
    va_end(ap);
    diag_error_count++;
}

void diag_note_at(SourceLoc loc, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    diag_vemit_at(loc, "note", sgr_note, fmt, ap);
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
