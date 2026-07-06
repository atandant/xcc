/* SPDX-License-Identifier: MIT */
/* expect: 42 */
int main(void) {
    long a[3] = {40, 2};
    return (int)(a[0] + a[1] + a[2]);
}
