/* SPDX-License-Identifier: MIT */
/* expect: 31 */
int main(void) {
    int x = 1;
    if (x) {
        if (x == 1)
            x = 31;
        else
            x = 12;
    } else {
        x = 9;
    }
    return x;
}
