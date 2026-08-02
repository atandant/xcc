/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* abi-role: callee */
/* abi-peer: long_double_caller_gcc.c */
long double xcc_long_double(int a, double b, long double c,
                            int d, float e, long double f)
{
    return (long double)a + (long double)b + c +
           (long double)d + (long double)e + f;
}
