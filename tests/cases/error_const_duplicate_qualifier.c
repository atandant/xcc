/* SPDX-License-Identifier: MIT */
/* expect-error: duplicate 'const' type qualifier */
typedef const int Constant;

int main(void)
{
    const Constant value = 1;
    return value;
}
