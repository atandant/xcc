/* SPDX-License-Identifier: MIT */
/* expect: 12 */
/* Section 2.4: nested complete struct member contributes its full size. */
struct Inner { int a; int b; };
struct Outer { struct Inner in; int c; };

int main(void) {
    struct Outer o;
    return sizeof(o);
}
