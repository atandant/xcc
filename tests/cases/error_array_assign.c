/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types assigning 'int *' to 'int[2]' */
int main(void) { int a[2]; int b[2]; a=b; return 0; }
