/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void) {
    short a[2][2] = {{1, 2}, {3, 4}};
    return a[1][0] + a[1][1];
}
