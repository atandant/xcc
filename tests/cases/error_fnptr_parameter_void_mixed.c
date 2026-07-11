/* SPDX-License-Identifier: MIT */
/* expect-error: parameter must not have void type */
int main(void)
{
    int (*fn)(void, int);
    return 0;
}
