/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int main(void) {
    short a[2] = {1 + 2, 3 + 4};
    return a[0] + a[1];
}
