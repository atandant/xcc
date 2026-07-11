/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int f(int (*p)[]);
int f(int (*p)[3])
{
    return (*p)[0];
}

int main(void)
{
    return 0;
}
