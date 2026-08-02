/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    long double values[3];
    values[0] = (long double)2;
    values[1] = (long double)4;
    values[2] = values[0] + values[1];
    return values[2] != (long double)6 || sizeof(values) != 48;
}
