/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int main(void) {
    int a[2][sizeof(char) + 2];
    a[1][2] = 9;
    return a[1][2];
}
