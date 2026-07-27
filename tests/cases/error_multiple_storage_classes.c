/* SPDX-License-Identifier: MIT */
/* expect-error: multiple storage classes in declaration specifiers */
static extern int x;
int main(void) { return x; }
