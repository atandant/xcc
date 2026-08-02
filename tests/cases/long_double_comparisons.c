/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    long double a = (long double)2;
    long double b = (long double)3;
    return !(a < b) || !(a <= b) || a == b || !(a != b) || a > b || a >= b;
}
