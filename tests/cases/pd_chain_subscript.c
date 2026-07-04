/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int main(void) { int a[2][3]; int (*p)[3]; a[1][2] = 5; p = a; return p[1][2]; }
