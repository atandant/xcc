/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int x; int *p; p = &x; return p == &x; }

