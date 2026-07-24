/* SPDX-License-Identifier: MIT */
/* expect: 30 */
int main(void)
{
    int a[3];
    int *p;
    int s;

    a[0] = 10;
    a[1] = 20;
    a[2] = 30;
    p = a;
    s = *p++;
    s = s + *p++;
    return s;
}
