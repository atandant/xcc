/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) { int x; int *p; p = &x; return (long)p - (long)p; }
