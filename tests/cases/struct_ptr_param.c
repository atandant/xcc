/* SPDX-License-Identifier: MIT */
/* expect: 42 */
/* Section 7.1: struct pointer parameter (unchanged pointer ABI) */
struct S { int x; };

void set(struct S *p) {
    p->x = 42;
}

int main(void) {
    struct S s;
    set(&s);
    return s.x;
}
