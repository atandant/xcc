/* SPDX-License-Identifier: MIT */
/* expect: 30 */
int main(void) { int a[2][3]; int (*p)[3]; a[0][0]=5; a[0][1]=10; a[0][2]=15; p=a; return (*p)[0] + (*p)[1] + (*p)[2]; }
