/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void)
{
    int a[3];
    int (*p)[];

    p = &a;
    (*p)[2] = 7;
    return a[2];
}
