/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int main(void)
{
    int i;
    int sum = 0;

    for (i = 0; i < 5; i++)
        sum = sum + i;
    return sum;
}
