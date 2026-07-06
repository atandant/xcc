/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) {
    unsigned long a[2] = {0};
    return (int)(a[0] + a[1]);
}
