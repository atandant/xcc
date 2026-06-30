/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int main(void) { int x; int *p; p = &x; *p = 9; return x; }
