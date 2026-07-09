/* SPDX-License-Identifier: MIT */
/* expect: 1 */
/* Union: members alias the same storage; low byte of int reads via char. */
union U { int i; unsigned char b; };

int main(void) {
    union U u;
    u.i = 257;        /* 0x101: low byte 1 */
    return u.b;
}
