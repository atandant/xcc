/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void) { int a[5]; long i; i = 1L; a[2] = 6; return a[i + 1L]; }
