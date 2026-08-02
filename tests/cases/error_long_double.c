/* SPDX-License-Identifier: MIT */
/* expect: 0 */
long double add(long double a, long double b) { return a + b; }
int main(void)
{
    long double a = (long double)9007199254740993UL;
    long double b = (long double)7;
    long double c = add(a, b);
    double d = (double)c;
    return sizeof(long double) != 16 || (unsigned long)c != 9007199254741000UL ||
           (unsigned long)(long double)d != 9007199254741000UL;
}
