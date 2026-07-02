/* SPDX-License-Identifier: MIT */
/* expect: 18 */
int main(void) { int a[3][4]; int i; int j; for (i = 0; i < 3; i = i + 1) for (j = 0; j < 4; j = j + 1) a[i][j] = i * 4 + j; return a[2][3] + a[1][3]; }
