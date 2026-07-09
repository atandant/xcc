/* SPDX-License-Identifier: MIT */
/* expect: 88 */
/* Union by-value return in RAX (8-byte union). */
union U { int i; long l; };

union U make(void) {
    union U u;
    u.i = 88;
    return u;
}

int main(void) {
    union U u = make();
    return u.i;
}
