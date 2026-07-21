/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int main(void)
{
    int i;
    int sum;

    i = 0;
    sum = 0;
again:
    i = i + 1;
    sum = sum + i;
    if (i < 4)
        goto again;
    return sum;
}
