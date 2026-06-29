/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int main(void) {
    int i = 0;
    int x = 0;
    for (; i < 4;) {
        x = x + 2;
        i = i + 1;
    }
    return x;
}
