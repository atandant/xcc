/* SPDX-License-Identifier: MIT */
/* expect-error: multiple storage classes in declaration specifiers */
int main(void) { register static int x; return 0; }
