/* SPDX-License-Identifier: MIT */
#include "diag.h"

#include <stdarg.h>
#include <stdio.h>

int diag_error_count = 0;

static void diag_vemit(SourceLoc loc, const char *kind, const char *fmt, va_list ap)
{
    fprintf(stderr, "%s:%d:%d: %s: ", g_filename, loc.line, loc.col, kind);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

void diag_error_at(SourceLoc loc, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    diag_vemit(loc, "error", fmt, ap);
    va_end(ap);
    diag_error_count++;
}

void diag_note_at(SourceLoc loc, const char *fmt, ...)
{
    if (diag_error_count == 0)
        return;

    va_list ap;
    va_start(ap, fmt);
    diag_vemit(loc, "note", fmt, ap);
    va_end(ap);
}
