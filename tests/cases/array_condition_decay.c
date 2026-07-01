/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) { int a[1]; a[0]=1; if (a) return 1; return 0; }
