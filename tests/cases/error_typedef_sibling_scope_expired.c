/* SPDX-License-Identifier: MIT */
/* expect-error: syntax error, unexpected IDENT, expecting ',' or ';' */
int main(void)
{
    {
        typedef int LocalType;
        LocalType x;
    }
    {
        LocalType y;
    }
    return 0;
}
