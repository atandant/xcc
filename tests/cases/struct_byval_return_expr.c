/* SPDX-License-Identifier: MIT */
/* expect: 7 */
/* Struct by-value: use returned struct via member access on call result. */
struct S { int x; int y; };

struct S make(int a, int b) {
    struct S s;
    s.x = a;
    s.y = b;
    return s;
}

int main(void) {
    return make(10, 7).y;
}
