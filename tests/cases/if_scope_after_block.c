/* SPDX-License-Identifier: MIT */
/* expect: 2 */
int main(void) {
    int x = 2;
    if (1) {
        int x = 5;
        x = x + 1;
    }
    return x;
}
