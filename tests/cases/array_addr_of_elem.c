/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void) { int a[2]; int *p; a[1]=3; p=&a[1]; return *p; }
