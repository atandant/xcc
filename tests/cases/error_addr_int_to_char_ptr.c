/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types assigning 'int *' to 'char *' */
int main(void) { int x; char *p; p = &x; return 0; }
