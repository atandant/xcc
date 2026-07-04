/* SPDX-License-Identifier: MIT */
/* expect: 11 */
int main(void) { int a[2][3]; int (*p)[3]; a[1][0] = 11; p = a; return (*(p + 1))[0]; }
