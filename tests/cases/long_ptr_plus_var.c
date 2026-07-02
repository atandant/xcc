/* SPDX-License-Identifier: MIT */
/* expect: 11 */
int main(void) { int a[5]; long i; i = 2L; a[2] = 11; return *(a + i); }
