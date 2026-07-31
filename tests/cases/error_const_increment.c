/* SPDX-License-Identifier: MIT */
/* expect-error: operand of increment/decrement is not a modifiable lvalue */
int main(void)
{
    const int value = 1;
    return ++value;
}
