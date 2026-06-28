/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void) {
    int x = 3;
    {
        int x = 7;
        x = x + 1;
    }
    return x;
}
