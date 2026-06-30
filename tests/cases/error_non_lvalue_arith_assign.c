/* SPDX-License-Identifier: MIT */
/* expect-error: assignment to non-lvalue */
int main(void) { int x; (x + 1) = 5; return 0; }
