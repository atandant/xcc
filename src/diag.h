#ifndef XCC_DIAG_H
#define XCC_DIAG_H

#include "token.h"

/* All diagnostics go to stderr (stdout is reserved for the .s output).
 * A nonzero count makes xcc exit nonzero. */
extern int diag_error_count;

void diag_error_at(SourceLoc loc, const char *fmt, ...);

#endif /* XCC_DIAG_H */
