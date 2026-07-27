/* SPDX-License-Identifier: MIT */
/* expect-error: conflicting linkage for 'value' */
int call(void) { return value(); }
static int value(void) { return 1; }
int main(void) { return call(); }
