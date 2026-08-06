/* SPDX-License-Identifier: MIT */
/* expect: 0 */

int main(void)
{
    int a = 3, b, *p = &a;
    unsigned i = 4, j = 5, k = 6;
    typedef int Number, *NumberPtr;
    Number n = 7;
    NumberPtr np = &n;

    b = *p + (int)(i + j + k);
    return b == 18 && *np == 7 ? 0 : 1;
}
