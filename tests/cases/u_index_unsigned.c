/* SPDX-License-Identifier: MIT */
/* expect: 77 */
int main(void) { int a[5]; unsigned int i; i = 2; a[2] = 77; return a[i]; }
