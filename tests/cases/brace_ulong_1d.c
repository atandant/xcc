/* SPDX-License-Identifier: MIT */
/* expect: 15 */
int main(void) {
    unsigned long a[2] = {7, 8};
    return (int)(a[0] + a[1]);
}
