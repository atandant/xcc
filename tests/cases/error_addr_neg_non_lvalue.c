/* SPDX-License-Identifier: MIT */
/* expect-error: cannot take address of non-lvalue */
int main(void) { int x; int *p; p = &(-x); return 0; }
