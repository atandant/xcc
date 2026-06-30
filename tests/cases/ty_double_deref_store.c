/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int main(void) { int x; int *p; int **pp; x = 0; p = &x; pp = &p; **pp = 8; return x; }
