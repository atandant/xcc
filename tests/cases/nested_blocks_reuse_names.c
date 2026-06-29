/* SPDX-License-Identifier: MIT */
/* expect: 16 */
int main(void) {
    int x = 1;
    {
        int x = 2;
        {
            int x = 8;
            x = x * 2;
            return x;
        }
    }
    return x;
}
