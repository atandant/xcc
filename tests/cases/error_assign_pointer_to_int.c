/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types */
int main(void) { int x; int *p; x = p; return 0; }
