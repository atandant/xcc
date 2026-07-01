/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types assigning 'int *' to 'int' */
int main(void) { int x; int *p; x = p; return 0; }
