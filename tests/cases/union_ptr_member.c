/* SPDX-License-Identifier: MIT */
/* expect: 7 */
/* Union: pointer member, accessed via `->` on a union pointer. */
union U { int *p; long l; };

int main(void) {
    int x;
    union U u;
    union U *up;
    x = 7;
    u.p = &x;
    up = &u;
    return *up->p;
}
