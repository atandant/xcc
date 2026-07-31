/* SPDX-License-Identifier: MIT */
/* expect: 6 */
struct S { int x; };
int main(void) {
    struct S s = {6};
    register struct S *p = &s;
    return *(&p->x);
}
