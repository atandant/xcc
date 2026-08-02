/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* abi-role: caller */
/* abi-peer: long_double_callee_gcc.c */
long double gcc_long_double(int, double, long double, int, float, long double);
int main(void)
{
    long double value = gcc_long_double(1, 2.5, (long double)3.25,
                                        4, 5.5F, (long double)6.75);
    return value != (long double)23;
}
