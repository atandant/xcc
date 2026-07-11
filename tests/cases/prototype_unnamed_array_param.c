/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int first(int [3]);

int first(int a[3])
{
    return a[0];
}

int main(void)
{
    int a[3];

    a[0] = 7;
    return first(a);
}
