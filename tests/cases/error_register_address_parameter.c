/* SPDX-License-Identifier: MIT */
/* expect-error: cannot take address of register object 'x' */
int f(register int x) { return *(&x); }
int main(void) { return f(1); }
