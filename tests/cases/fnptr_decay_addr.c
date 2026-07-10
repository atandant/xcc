/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int f(void) { return 7; }
int main(void) {
    int (*a)(void);
    int (*b)(void);
    a = f;
    b = &f;
    return a() + b() - 7;
}
