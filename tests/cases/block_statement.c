/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int main(void) {
    int x = 1;
    {
        x = x + 3;
        x = x + 5;
    }
    return x;
}
