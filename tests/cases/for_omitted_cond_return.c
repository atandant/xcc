/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int main(void) {
    int i = 0;
    for (;; i = i + 1) {
        if (i == 5)
            return i;
    }
    return 9;
}
