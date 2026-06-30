/* SPDX-License-Identifier: MIT */
/* expect-error: dereference of void pointer */
int main(void) { int x; void *v; v = &x; return *v; }
