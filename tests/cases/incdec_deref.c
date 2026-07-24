/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void)
{
    int x = 5;
    int *p;

    p = &x;
    (*p)++;
    return x;
}
