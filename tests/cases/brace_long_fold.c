/* SPDX-License-Identifier: MIT */
/* expect: 16 */
int main(void) {
    long a[2] = {5 * 2, 3 + 3};
    return (int)(a[0] + a[1]);
}
