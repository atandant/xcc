/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int main(void) { int a[2]; int *p; a[0]=8; p=&a[0]; return *p; }
