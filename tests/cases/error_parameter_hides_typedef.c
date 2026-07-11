/* SPDX-License-Identifier: MIT */
/* expect-error: syntax error, unexpected IDENT */
typedef int Value;

int f(int Value, Value other);

int main(void)
{
    return 0;
}
