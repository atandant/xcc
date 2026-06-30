/* SPDX-License-Identifier: MIT */
/* expect-error: cannot take address of non-lvalue */
int main(void) { int *p; p = &3; return 0; }
