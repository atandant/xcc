/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void) {
    int x = 3;
    {
        int x = 7;
        return x;
    }
    return x;
}
