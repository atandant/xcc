/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int pick(int x[4]) { return x[3]; }
int main(void) { int a[4]; a[3]=4; return pick(a); }
