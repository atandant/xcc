/* SPDX-License-Identifier: MIT */
/* expect: 55 */
/* Union: whole-object union-to-union assignment (memcpy path). */
union U { int i; long l; };

int main(void) {
    union U a;
    union U b;
    a.i = 55;
    b = a;
    return b.i;
}
