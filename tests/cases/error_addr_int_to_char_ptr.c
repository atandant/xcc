/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types */
int main(void) { int x; char *p; p = &x; return 0; }
