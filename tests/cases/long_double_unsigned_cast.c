/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    unsigned long n = 18446744073709551615UL;
    long double value = (long double)n;
    return (unsigned long)value != n ||
           (unsigned int)(long double)4000000200U != 4000000200U;
}
