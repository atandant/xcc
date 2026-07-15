/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) {
    int x;
    x = 7;
    while ((x = 0)) {
    }
    return x;
}
