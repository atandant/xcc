/* SPDX-License-Identifier: MIT */
/* expect: 8 */
/* Section 2.4: pointer to struct is a scalar of pointer size. */
struct Point { int x; int y; };

int main(void) {
    struct Point *p;
    return sizeof(p);
}
