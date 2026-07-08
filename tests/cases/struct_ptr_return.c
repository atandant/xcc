/* SPDX-License-Identifier: MIT */
/* expect: 17 */
/* Section 7.1: struct pointer return (unchanged pointer ABI) */
struct S { int x; };

struct S *identity(struct S *p) {
    return p;
}

int main(void) {
    struct S s;
    s.x = 17;
    return identity(&s)->x;
}
