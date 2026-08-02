/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    long double value = (long double)8;
    value += (long double)4;
    value *= (long double)3;
    value -= (long double)6;
    value /= (long double)5;
    return value != (long double)6;
}
