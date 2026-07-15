/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int sum_skip(int n, int skip)
{
    int i;
    int total;

    total = 0;
    for (i = 0; i < n; i = i + 1) {
        if (i == skip)
            continue;
        total = total + i;
    }
    return total;
}

int main(void)
{
    return sum_skip(5, 2);
}
