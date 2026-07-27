/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting types for 'value' */
static int value(void);
static long value(void);
int main(void) { return 0; }
