/* SPDX-License-Identifier: MIT */
/* expect: 8 */

typedef union { int value; long wide; } Number;

int main(void)
{
    Number number;
    number.wide = 8;
    return number.value;
}
