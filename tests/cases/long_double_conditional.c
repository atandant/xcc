/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    int choose = 1;
    long double a = (long double)7;
    long double b = (long double)9;
    return (choose ? a : b) != a || (!choose ? a : b) != b;
}
