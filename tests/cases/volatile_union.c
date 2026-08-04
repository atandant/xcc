/* SPDX-License-Identifier: MIT */
/* expect: 45 */
union Value { int integer; unsigned long wide; };
int main(void)
{
    volatile union Value value;
    value.integer = 45;
    return value.integer;
}
