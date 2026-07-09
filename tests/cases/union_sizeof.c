/* SPDX-License-Identifier: MIT */
/* expect: 8 */
/* Union: sizeof is the size of the largest member (long = 8). */
union U { int i; long l; char c; };

int main(void) {
    union U u;
    return sizeof(u);
}
