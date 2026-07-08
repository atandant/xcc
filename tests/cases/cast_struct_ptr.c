/* SPDX-License-Identifier: MIT */
/* expect: 9 */
/* Section 6.2: cast through void * to struct pointer */
struct S { int x; };

int main(void) {
    struct S s;
    void *q;

    s.x = 9;
    q = (void *)&s;
    return ((struct S *)q)->x;
}
