/* SPDX-License-Identifier: MIT */
/* expect: 42 */
/* Struct by-value parameter (<=8 bytes) passed in one GPR. */
struct S { int x; };

int take(struct S s) {
    return s.x;
}

int main(void) {
    struct S s;
    s.x = 42;
    return take(s);
}
