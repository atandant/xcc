/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int main(void) {
    int a[2] = {(1, 2), 3};
    return a[0] + a[1];
}
