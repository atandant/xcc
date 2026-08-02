/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void)
{
    long double value = (long double)4;
    long double old = value++;
    --value;
    return old != (long double)4 || value != (long double)4;
}
