/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types */
int main(void) { int x; int **pp; pp = &x; return 0; }
