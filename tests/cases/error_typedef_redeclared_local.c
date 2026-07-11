/* SPDX-License-Identifier: MIT */
/* expect-error: redeclared 'Value' as different kind of symbol */
int main(void)
{
    typedef int Value;
    int Value;
    return 0;
}
