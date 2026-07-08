/* SPDX-License-Identifier: MIT */
/* expect: 5 */
/* Section 4.1: partial nested struct init zero-fills inner tail */
struct Inner { int a; int b; };
struct Outer { struct Inner in; int c; };

int main(void) {
    struct Outer o = { { 5 } };
    return o.in.a + o.in.b + o.c;
}
