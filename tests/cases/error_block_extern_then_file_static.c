/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting linkage for 'value' */
void declare(void) { extern int value; }
static int value;
