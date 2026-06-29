/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void) {
    int i = 0;
    for (;;) {
        i = i + 1;
        if (i == 3)
            return i;
    }
    return 0;
}
