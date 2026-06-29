/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int main(void) {
    int i = 0;
    int x = 0;
    while (i < 5) {
        if (i < 3)
            x = x + i;
        else
            x = x + 1;
        i = i + 1;
    }
    return x;
}
