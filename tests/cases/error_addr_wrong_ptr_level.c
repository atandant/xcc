/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types assigning 'int *' to 'int * *' */
int main(void) { int x; int **pp; pp = &x; return 0; }
