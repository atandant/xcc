/* SPDX-License-Identifier: MIT */
/* expect-error: cannot take address of member of register object */
struct S { int x; };
int main(void) { register struct S s; return *(&s.x); }
