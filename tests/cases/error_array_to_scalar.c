/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types assigning 'int *' to 'int' */
int main(void) { int a[2]; int x; x=a; return 0; }
