/* SPDX-License-Identifier: MIT */
/* expect: 3 */
/* Section 6.2: (struct S *) cast in abstract declarator */
struct S { int x; };

struct S *id(struct S *p) {
    return p;
}

int main(void) {
    struct S s;
    void *v;

    s.x = 3;
    v = (void *)&s;
    return id((struct S *)v)->x;
}
