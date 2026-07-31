/* SPDX-License-Identifier: MIT */
/* expect-error: cannot take address of member of register object */
struct Inner { int x; };
struct Outer { struct Inner inner; };
int main(void) { register struct Outer s; return *(&s.inner.x); }
