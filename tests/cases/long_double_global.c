/* SPDX-License-Identifier: MIT */
/* expect: 0 */
long double value = 37.25L;
static long double values[2] = { 1.25L, 2.75L };
int main(void)
{
    static long double local = 4.5L;
    return value != 37.25L || values[0] + values[1] != 4.0L || local != 4.5L;
}
