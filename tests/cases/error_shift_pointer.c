/* SPDX-License-Identifier: MIT */
/* expect-error: invalid operands to shift operator */
int main(void) { int x; int *p; p = &x; return p << 1; }
