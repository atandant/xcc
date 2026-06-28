/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) {
    int x = 5;
    {
        int x = x;
        return x == 5;
    }
}
