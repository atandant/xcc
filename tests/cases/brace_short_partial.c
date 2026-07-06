/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int main(void) {
    short a[4] = {5, 7};
    return a[0] + a[1] + a[2] + a[3];
}
