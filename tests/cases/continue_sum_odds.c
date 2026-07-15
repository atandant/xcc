/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int sum_odds(int n)
{
    int i;
    int total;

    total = 0;
    for (i = 0; i < n; i = i + 1) {
        if ((i % 2) == 0)
            continue;
        total = total + i;
    }
    return total;
}

int main(void)
{
    return sum_odds(6);
}
