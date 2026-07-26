/* SPDX-License-Identifier: MIT */
/* expect: 3 */
struct Bits { int a : 3; int b : 5; };
struct Bits bits = { 1, 2 };
int main(void) { return bits.a + bits.b; }
