/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void) {
    int x = 9;
    if (1)
        x = 3;
    return x;
}
