/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void) {
    int i = 1;
    int x = 1;
    for (; i < 4; i = i + 1)
        x = x * i;
    return x;
}
