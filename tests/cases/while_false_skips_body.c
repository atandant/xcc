/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void) {
    int x = 7;
    while (0)
        x = 3;
    return x;
}
