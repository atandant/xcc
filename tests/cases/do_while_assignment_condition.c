/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void)
{
    int i;
    int sum;

    i = 0;
    sum = 0;
    do {
        sum = sum + i;
    } while ((i = i + 1) < 4);
    return sum;
}
