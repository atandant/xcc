/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int main(void) {
    int x = 2;
    if (1) {
        int x = 5;
        return x;
    }
    return x;
}
