/* SPDX-License-Identifier: MIT */
/* expect: 0 */

int left = 6;
int right = 7;

int main(void)
{
    static int a = 3, b = 4;
    register int c = a + b, d = c + 1;
    extern int left, right;

    return a + b + c + d + left + right == 35 ? 0 : 1;
}
