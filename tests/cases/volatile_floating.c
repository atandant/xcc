/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    volatile float single;
    volatile double wide;
    volatile long double extended;
    single = 1.5f;
    wide = 2.25;
    extended = 3.75L;
    return single + wide + extended != 7.5L;
}
