/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int main(void) { int a[2][3]; a[1][2] = 7; a[0][0] = 5; return a[1][2] + a[0][0]; }
