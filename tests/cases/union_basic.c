/* SPDX-License-Identifier: MIT */
/* expect: 42 */
/* Union: write and read back the same member. */
union U { int i; char c; };

int main(void) {
    union U u;
    u.i = 42;
    return u.i;
}
