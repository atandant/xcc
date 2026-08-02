/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    float f = 6.25F;
    long double value = (long double)f;
    return (float)value != f;
}
