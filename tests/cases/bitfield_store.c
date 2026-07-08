/* SPDX-License-Identifier: MIT */
/* expect: 253 */
/* Section 5.3: store through bit-field lvalue */
struct P { int a : 3; int b : 5; };

int main(void) {
    struct P p;
    p.a = 5;
    p.b = 0;
    return p.a;
}
