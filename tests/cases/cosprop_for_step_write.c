/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) {
    int x;
    int i;
    x = 7;
    i = 0;
    for (; i < 1; x = 3)
        i = i + 1;
    if (x == 3)
        return 0;
    return 1;
}
