/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int main(void) {
    int i = 0;
    int j = 0;
    int x = 0;
    for (i = 0; i < 3; i = i + 1) {
        for (j = 0; j < 4; j = j + 1)
            x = x + 1;
    }
    return x;
}
