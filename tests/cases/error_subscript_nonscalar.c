/* SPDX-License-Identifier: MIT */
/* expect-error: cannot dereference non-pointer type 'int' */
int main(void) { int x; return x[0]; }
