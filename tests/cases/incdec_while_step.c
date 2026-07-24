/* SPDX-License-Identifier: MIT */
/* expect: 10 */
int main(void)
{
    int i = 0;
    int sum = 0;

    while (i < 5) {
        sum = sum + i;
        i++;
    }
    return sum;
}
