/* SPDX-License-Identifier: MIT */
/* expect: 0 */
long double mix(int a, double b, long double c, int d, float e, long double f)
{
    return (long double)a + (long double)b + c +
           (long double)d + (long double)e + f;
}
int main(void)
{
    return mix(1, 2.5, (long double)3.25, 4, 5.5F,
               (long double)6.75) != (long double)23;
}
