/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int a[2][3]; int (*p)[3]; p = a; return (p + 1) - p; }
