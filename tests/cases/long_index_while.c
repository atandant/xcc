/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int main(void) { int a[4]; long i; long s; i = 0L; s = 0L; while (i < 4L) { a[i] = i + 1L; s = s + a[i]; i = i + 1L; } return s; }
