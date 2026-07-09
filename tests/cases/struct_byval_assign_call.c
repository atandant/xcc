/* SPDX-License-Identifier: MIT */
/* expect: 33 */
/* Struct by-value assignment from call result. */
struct S { int x; };

struct S make(int v) {
    struct S s;
    s.x = v;
    return s;
}

int main(void) {
    struct S a;
    struct S b;
    a = make(11);
    b = make(22);
    return a.x + b.x;
}
