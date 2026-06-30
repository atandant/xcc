/* SPDX-License-Identifier: MIT */
/* expect-error: invalid operands to relational operator */
int main(void) { int *a; int *b; return a < b; }
