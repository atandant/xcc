/* SPDX-License-Identifier: MIT */
/* expect: 90 */
int main(void) { char a[5]; long i; i = 3L; a[3] = 90; return a[i]; }
