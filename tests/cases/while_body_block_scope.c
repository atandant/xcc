/* SPDX-License-Identifier: MIT */
/* expect: 11 */
int main(void) {
    int i = 0;
    int x = 0;
    while (i < 2) {
        int y = i + 2;
        x = x + y;
        i = i + 1;
    }
    {
        int y = 6;
        x = x + y;
    }
    return x;
}
