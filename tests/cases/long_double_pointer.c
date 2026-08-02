/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    long double value = (long double)11;
    long double *p = &value;
    *p = *p + (long double)5;
    return value != (long double)16;
}
