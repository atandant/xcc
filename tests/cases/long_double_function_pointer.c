/* SPDX-License-Identifier: MIT */
/* expect: 0 */
long double add(long double a, long double b) { return a + b; }
int main(void)
{
    long double (*fn)(long double, long double) = add;
    return fn((long double)19, (long double)23) != (long double)42;
}
