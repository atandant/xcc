/* SPDX-License-Identifier: MIT */
/* expect: 15 */
int main(void) { int a[5]; a[2] = 15; return *(2L + a); }
