/* SPDX-License-Identifier: MIT */
/* expect: 15 */
int main(void)
{
    int i;
    int sum;

    i = 1;
    sum = 0;
    do {
        sum = sum + i;
        i = i + 1;
    } while (i < 6);
    return sum;
}
