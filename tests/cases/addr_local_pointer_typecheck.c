/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) { int x; int *p; int **pp; p = &x; pp = &p; return 0; }
