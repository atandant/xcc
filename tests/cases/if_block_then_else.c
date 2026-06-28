/* SPDX-License-Identifier: MIT */
/* expect: 13 */
int main(void) {
    int x = 4;
    if (x < 3) {
        x = 99;
    } else {
        x = x + 9;
    }
    return x;
}
