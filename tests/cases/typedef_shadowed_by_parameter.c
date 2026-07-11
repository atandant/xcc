/* SPDX-License-Identifier: MIT */
/* expect: 7 */
typedef int Value;

int identity(int Value)
{
    return Value;
}

int main(void)
{
    return identity(7);
}
