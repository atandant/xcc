/* SPDX-License-Identifier: MIT */
/* expect: 60 */
int main(void) {
    long a[3] = {10, 20, 30};
    return (int)(a[0] + a[1] + a[2]);
}
