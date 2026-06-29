/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) {
    int x = 2147483647;
    return x < 2147483648;
}
