/* SPDX-License-Identifier: MIT */
/* expect-error: array parameter has incomplete element type 'int[0]' */
int f(int a[][]);

int main(void)
{
    return 0;
}
