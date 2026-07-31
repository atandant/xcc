/* SPDX-License-Identifier: MIT */
/* expect-error: multiple storage classes in declaration specifiers */
int main(void) { auto register int x; return 0; }
