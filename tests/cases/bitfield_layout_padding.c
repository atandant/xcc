/* SPDX-License-Identifier: MIT */
/* expect: 3 */
/* Section 5.2: `: 0` forces next bitfield into a new unit (d at offset 4) */
struct B { int a : 3; int b : 5; int : 0; int d : 4; };

int main(void) {
    struct B x;
    x.a = 1;
    x.b = 2;
    x.d = 0;
    return x.a + x.b;
}
