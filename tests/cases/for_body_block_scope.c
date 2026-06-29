/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int main(void) {
    int i = 0;
    int x = 0;
    for (i = 0; i < 3; i = i + 1) {
        int y = i + 1;
        x = x + y;
    }
    {
        int y = 4;
        x = x + y;
    }
    return x;
}
