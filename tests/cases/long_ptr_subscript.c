/* SPDX-License-Identifier: MIT */
/* expect: 99 */
int main(void) { int a[4]; a[2L] = 99; return a[2L]; }
