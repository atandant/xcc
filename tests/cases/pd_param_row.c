/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int pick(int (*row)[3]) { return row[0][1] + row[0][2]; }
int main(void) { int a[1][3]; a[0][1] = 2; a[0][2] = 4; return pick(a); }
