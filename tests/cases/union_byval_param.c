/* SPDX-License-Identifier: MIT */
/* expect: 55 */
/* Union by-value parameter: same SysV rules as struct. */
union U { int i; long l; };

int take(union U u) {
    return u.i;
}

int main(void) {
    union U u;
    u.i = 55;
    return take(u);
}
