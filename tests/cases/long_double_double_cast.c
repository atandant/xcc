/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    double d = 12345.75;
    long double value = (long double)d;
    return (double)value != d;
}
