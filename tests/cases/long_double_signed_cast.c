/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    long double a = (long double)-123456789;
    long double b = (long double)-9007199254740993L;
    return (int)a != -123456789 || (long)b != -9007199254740993L;
}
