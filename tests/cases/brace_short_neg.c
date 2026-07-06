/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) {
    short a[2] = {-1, 2};
    return a[0] + a[1];
}
