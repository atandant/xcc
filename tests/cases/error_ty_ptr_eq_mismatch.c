/* SPDX-License-Identifier: MIT */
/* expect-error: invalid operands to comparison */
int main(void) { int x; int *a; char *b; a = &x; b = &x; return a == b; }
