/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    long double zero = (long double)0;
    long double one = (long double)1;
    return !!zero || !one || !(one && one) || (zero || zero);
}
