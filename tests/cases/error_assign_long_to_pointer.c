/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types assigning 'long' to 'int *' */
int main(void) { int *p; long x; p = x; return 0; }
