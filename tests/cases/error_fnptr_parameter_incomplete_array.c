/* SPDX-License-Identifier: MIT */
/* expect-error: array parameter has incomplete element type 'int[0]' */
int main(void)
{
    int (*fn)(int a[][]);
    return 0;
}
