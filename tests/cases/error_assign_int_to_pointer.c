/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types assigning 'int' to 'int *' */
int main(void) { int *p; p = 5; return 0; }
