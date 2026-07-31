/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void) {
    int x = 3;
    { register int x = 7; if (x != 7) return 1; }
    return *(&x) + 3;
}
