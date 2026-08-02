/* SPDX-License-Identifier: MIT */
/* expect: 0 */
long double add(long double a, long double b) { return a + b; }
int main(void)
{
    return add((long double)12, (long double)30) != (long double)42;
}
