/* SPDX-License-Identifier: MIT */
long double xcc_long_double(int, double, long double, int, float, long double);
int main(void)
{
    return xcc_long_double(1, 2.5, 3.25L, 4, 5.5F, 6.75L) != 23.0L;
}
