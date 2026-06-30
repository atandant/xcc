/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int x; int y; int *a; int *b; a = &x; b = &y; return a != b; }
