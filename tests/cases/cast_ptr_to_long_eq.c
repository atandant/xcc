/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int x; int *p; long a; long b; p = &x; a = (long)p; b = (long)p; return a == b; }
