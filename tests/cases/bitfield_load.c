/* SPDX-License-Identifier: MIT */
/* expect: 7 */
/* Section 5.3: load packed bit-fields */
struct P { int a : 3; int b : 5; };

int main(void) {
    struct P p;
    p.a = 3;
    p.b = 4;
    return p.a + p.b;
}
