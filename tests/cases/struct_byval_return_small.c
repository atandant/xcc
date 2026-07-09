/* SPDX-License-Identifier: MIT */
/* expect: 7 */
/* Struct by-value return (<=8 bytes) in RAX. */
struct S { int x; };

struct S make(void) {
    struct S s;
    s.x = 7;
    return s;
}

int main(void) {
    struct S s = make();
    return s.x;
}
