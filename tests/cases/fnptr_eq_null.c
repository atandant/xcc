/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int noop(void) { return 0; }
int main(void) {
    int (*fp)(void) = 0;
    if (fp == 0) {
        fp = noop;
        return fp() == 0;
    }
    return 0;
}
