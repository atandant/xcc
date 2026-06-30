/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int x; int *a; int *b; a = &x; b = &x; return a == b; }
