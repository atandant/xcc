/* SPDX-License-Identifier: MIT */
/* expect: 12 */
/* Section 4.1: nested struct brace initialization */
struct Inner { int a; int b; };
struct Outer { struct Inner in; int c; };

int main(void) {
    struct Outer o = { { 5, 7 }, 0 };
    return o.in.a + o.in.b;
}
