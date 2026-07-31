/* SPDX-License-Identifier: MIT */
#ifndef XCC_DIAG_H
#define XCC_DIAG_H

#include "token.h"
#include <stdio.h>

/* All diagnostics go to stderr (stdout is reserved for the .s output).
 * Errors increment diag_error_count and make xcc exit nonzero.
 * Warnings increment diag_warning_count but do not affect the exit code
 * unless promoted via -Werror. Notes never increment either count. */
extern int diag_error_count;
extern int diag_warning_count;

typedef enum {
    W_IMPLICIT_FUNCTION_DECLARATION,
    W_CALL_WITHOUT_PROTOTYPE,
    W_CHAR_CONSTANT_OVERFLOW,
    W_CHAR_VALUE_NARROWING,
    W_RETURN_TYPE,
    W_OLD_STYLE_DEFINITION,
    W_IMPLICIT_VOID_POINTER,
    W_SELF_REFERENTIAL_INITIALIZER,
    W_COUNT
} DiagWarnId;

void diag_error_at(SourceLoc loc, const char *fmt, ...);
void diag_warning_at(SourceLoc loc, const char *fmt, ...);
void diag_note_at(SourceLoc loc, const char *fmt, ...);

/* Gated warning: respects per-warning enable / -Werror state. */
void diag_warn(DiagWarnId id, SourceLoc loc, const char *fmt, ...);

int diag_warn_enabled(DiagWarnId id);
int diag_warn_as_error(DiagWarnId id);
int diag_warn_default_enabled(DiagWarnId id);
int diag_warn_in_wall_group(DiagWarnId id);
const char *diag_warn_name(DiagWarnId id);
void diag_set_warn_enabled(DiagWarnId id, int enabled);
void diag_set_warn_as_error(DiagWarnId id, int as_error);
void diag_set_warn_error_suppress(DiagWarnId id, int suppress);

/* CLI: 0 = handled, 1 = unknown warning name, 2 = malformed flag. */
int diag_apply_warn_flag(const char *arg);
void diag_disable_all_warnings(void);
void diag_enable_wall_warnings(void);
void diag_print_warnings_help(FILE *f);

/* Driver / fatal messages without a source location (no caret). */
void diag_error(const char *fmt, ...);
void diag_fatal(const char *fmt, ...);

#endif /* XCC_DIAG_H */
