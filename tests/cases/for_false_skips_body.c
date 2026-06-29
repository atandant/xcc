/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int main(void) {
    int i = 0;
    int x = 4;
    for (i = 0; 0; i = i + 1)
        x = 9;
    return x + i;
}
