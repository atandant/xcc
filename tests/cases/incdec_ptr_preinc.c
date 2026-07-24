/* SPDX-License-Identifier: MIT */
/* expect: 40 */
int main(void)
{
    int a[3];
    int *p;
    int x;

    a[0] = 10;
    a[1] = 20;
    a[2] = 30;
    p = a;
    x = *++p;
    return x + *p;
}
