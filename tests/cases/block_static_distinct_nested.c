/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) {
    static int value = 2;
    { static int value = 5; if (value != 5) return 1; }
    return value - 2;
}
