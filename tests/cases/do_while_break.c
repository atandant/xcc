/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void)
{
    int i;
    int sum;

    i = 0;
    sum = 0;
    do {
        if (i == 3)
            break;
        sum = sum + i;
        i = i + 1;
    } while (i < 10);
    return sum;
}
