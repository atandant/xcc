/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int x; int *p; p = &x; return (int)p == (int)p; }
