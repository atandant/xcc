/* SPDX-License-Identifier: MIT */
#ifndef XCC_DIAG_H
#define XCC_DIAG_H

#include "token.h"

/* All diagnostics go to stderr (stdout is reserved for the .s output).
 * A nonzero count makes xcc exit nonzero. Notes never increment the count. */
extern int diag_error_count;

/* Source lines for carets (1-based line numbers). NULL disables carets. */
void diag_set_source(char **lines, int nlines);

void diag_error_at(SourceLoc loc, const char *fmt, ...);
void diag_note_at(SourceLoc loc, const char *fmt, ...);

/* Driver / fatal messages without a source location (no caret). */
void diag_error(const char *fmt, ...);
void diag_fatal(const char *fmt, ...);

#endif /* XCC_DIAG_H */
