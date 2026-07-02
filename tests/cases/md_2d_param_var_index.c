/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int get(int a[4][5], int i, int j) { return a[i][j]; }
int main(void) { int a[4][5]; a[2][1] = 8; return get(a, 2, 1); }
