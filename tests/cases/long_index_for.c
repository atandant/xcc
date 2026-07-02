/* SPDX-License-Identifier: MIT */
/* expect: 15 */
int main(void) { int a[5]; long i; long s; s = 0L; for (i = 1L; i <= 5L; i = i + 1L) { a[i - 1L] = i; s = s + a[i - 1L]; } return s; }
