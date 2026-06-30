/* SPDX-License-Identifier: MIT */
/* expect-error: assignment to non-lvalue */
int main(void) { int x; -x = 2; return 0; }
