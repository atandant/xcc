/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int main(void) {
    int x = 9;
    if (0)
        x = 3;
    return x;
}
