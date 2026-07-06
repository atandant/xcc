/* SPDX-License-Identifier: MIT */
/* expect: 60 */
int main(void) {
    unsigned char a[3] = {10, 20, 30};
    return a[0] + a[1] + a[2];
}
