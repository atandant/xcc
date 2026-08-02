/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    long double a = (long double)21;
    long double b = (long double)4;
    return a + b != (long double)25 || a - b != (long double)17 ||
           a * b != (long double)84 || a / b != (long double)5.25;
}
