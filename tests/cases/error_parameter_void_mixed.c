/* SPDX-License-Identifier: MIT */
/* expect-error: parameter must not have void type */
int f(void, int x);

int main(void)
{
    return 0;
}
