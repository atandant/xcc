/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int main(void) {
    int a[][2] = {1, 2, 3, 4, 5};
    return a[2][0] + a[2][1];
}
