/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting linkage for 'value' */
extern int value(void);
static int value(void) { return 1; }
int main(void) { return value(); }
