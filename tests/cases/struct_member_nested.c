/* SPDX-License-Identifier: MIT */
/* expect: 12 */
/* Section 3: chained member access on nested struct */
struct Inner { int a; int b; };
struct Outer { struct Inner in; int c; };

int main(void) {
    struct Outer o;
    o.in.a = 12;
    o.in.b = 0;
    o.c = 0;
    return o.in.a;
}
