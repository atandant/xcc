/* SPDX-License-Identifier: MIT */
/* expect: 55 */
int main(void) { int a[4]; long i; i = 1L; a[i] = 55; return a[i]; }
