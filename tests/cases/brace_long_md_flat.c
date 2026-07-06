/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int main(void) {
    long a[2][2] = {1, 2, 3, 4};
    return (int)a[1][1];
}
