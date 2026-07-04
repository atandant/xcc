/* SPDX-License-Identifier: MIT */
/* expect-error: array declarator not allowed on pointer type */
int main(void) { int *p[3]; return 0; }
