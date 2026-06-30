/* SPDX-License-Identifier: MIT */
/* expect-error: returning 'int *' from a function returning 'int' */
int f(void) { int *p; return p; }
int main(void) { return 0; }
