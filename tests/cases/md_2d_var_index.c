/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void) { int a[2][3]; int i; a[1][1] = 3; i = 1; return a[i][i]; }
