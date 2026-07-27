/* SPDX-License-Identifier: MIT */
/* expect-error: redefinition of 'value' */
static int value(void) { return 1; }
static int value(void) { return 2; }
int main(void) { return value(); }
