/* SPDX-License-Identifier: MIT */
#ifndef XCC_DIAG_H
#define XCC_DIAG_H

#include "token.h"

/* All diagnostics go to stderr (stdout is reserved for the .s output).
 * Errors increment diag_error_count and make xcc exit nonzero.
 * Warnings increment diag_warning_count but do not affect the exit code.
 * Notes never increment either count. */
extern int diag_error_count;
extern int diag_warning_count;

typedef enum {
    W_IMPLICIT_FUNCTION_DECLARATION,
    W_UNPROTOTYPED_FUNCTION_CALL,
    W_INT_TO_CHAR_OVERFLOW,
    W_INT_TO_CHAR_CONVERSION,
    W_RETURN_TYPE,
    W_OLD_STYLE_FUNCTION_DEFINITION,
    W_POINTER_CONVERSION,
    W_INIT_FROM_SELF,
    W_COUNT
} DiagWarnId;

/* Source lines for carets (1-based line numbers). NULL disables carets. */
void diag_set_source(char **lines, int nlines);

void diag_error_at(SourceLoc loc, const char *fmt, ...);
void diag_warning_at(SourceLoc loc, const char *fmt, ...);
void diag_note_at(SourceLoc loc, const char *fmt, ...);

/* Gated warning: respects per-warning enable / -Werror state (future CLI). */
void diag_warn(DiagWarnId id, SourceLoc loc, const char *fmt, ...);

int diag_warn_enabled(DiagWarnId id);
int diag_warn_as_error(DiagWarnId id);
const char *diag_warn_name(DiagWarnId id);
void diag_set_warn_enabled(DiagWarnId id, int enabled);
void diag_set_warn_as_error(DiagWarnId id, int as_error);

/* Driver / fatal messages without a source location (no caret). */
void diag_error(const char *fmt, ...);
void diag_fatal(const char *fmt, ...);

#endif /* XCC_DIAG_H */
