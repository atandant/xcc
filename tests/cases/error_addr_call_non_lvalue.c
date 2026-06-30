/* SPDX-License-Identifier: MIT */
/* expect-error: cannot take address of non-lvalue */
int f(void) { return 1; }
int main(void) { int *p; p = &f(); return 0; }
