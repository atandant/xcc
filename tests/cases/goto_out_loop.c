/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void)
{
    int i;
    int sum;

    i = 0;
    sum = 0;
    while (i < 10) {
        if (i == 4)
            goto done;
        sum = sum + i;
        i = i + 1;
    }
done:
    return sum;
}
