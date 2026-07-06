/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int main(void) {
    int a[2] = {1 + 2, 3 + 3};
    return a[0] + a[1];
}
