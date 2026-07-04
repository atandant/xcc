/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int main(void) { int a[2][3]; int (*p)[3]; a[0][2] = 9; p = a; return (*p)[2]; }
