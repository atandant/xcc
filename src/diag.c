/* SPDX-License-Identifier: MIT */
#include "diag.h"

#include <stdarg.h>
#include <stdio.h>

int diag_error_count = 0;

void diag_error_at(SourceLoc loc, const char *fmt, ...)
{
    fprintf(stderr, "%s:%d:%d: error: ", g_filename, loc.line, loc.col);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    fputc('\n', stderr);
    diag_error_count++;
}
