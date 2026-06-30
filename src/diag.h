/* SPDX-License-Identifier: MIT */
#ifndef XCC_DIAG_H
#define XCC_DIAG_H

#include "token.h"

/* All diagnostics go to stderr (stdout is reserved for the .s output).
 * A nonzero count makes xcc exit nonzero. Notes never increment the count. */
extern int diag_error_count;

void diag_error_at(SourceLoc loc, const char *fmt, ...);

/* Supplementary context printed after an error. No-op unless at least one error
 * has already been reported (diag_error_count > 0). Never emits "warning:". */
void diag_note_at(SourceLoc loc, const char *fmt, ...);

#endif /* XCC_DIAG_H */
