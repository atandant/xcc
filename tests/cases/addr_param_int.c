/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int getaddr(int x) { int *p; p = &x; return 0; }
int main(void) { return getaddr(5); }
