/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void) { int a[2][3]; int (*p)[3]; a[0][0]=1; a[1][0]=1; a[1][1]=1; p = (int (*)[3])a; return (*p)[0] + (*(p + 1))[0] + (*(p + 1))[1]; }
