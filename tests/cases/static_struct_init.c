/* SPDX-License-Identifier: MIT */
/* expect: 30 */
struct S { int a; int b; };
static struct S s = { 10, 20 };
int main(void) { return s.a + s.b; }
