/* SPDX-License-Identifier: MIT */
/* expect-error: operand of increment/decrement is not a modifiable lvalue */
int main(void)
{
    return ++5;
}
