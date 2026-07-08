/* SPDX-License-Identifier: MIT */
/* expect: 3 */
/* Section 5.4: brace initialization of bit-fields */
struct P { int a : 3; int b : 5; };

int main(void) {
    struct P p = { 1, 2 };
    return p.a + p.b;
}
