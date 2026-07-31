/* SPDX-License-Identifier: MIT */
/* expect-error: cannot take address of register object 'x' */
int main(void) { register int x; return *(&x); }
