/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int main(void) {
    int x = 1;
    if (1)
        if (0)
            x = 3;
        else
            x = 4;
    return x;
}
