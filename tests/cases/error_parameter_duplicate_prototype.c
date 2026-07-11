/* SPDX-License-Identifier: MIT */
/* expect-error: redefinition of parameter 'x' */
int f(int x, int x);

int main(void)
{
    return 0;
}
