/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int sum_until_break(int n, int stop)
{
    int i;
    int total;

    total = 0;
    i = 0;
    while (i < n) {
        if (i == stop)
            break;
        total = total + i;
        i = i + 1;
    }
    return total;
}

int main(void)
{
    return sum_until_break(10, 4);
}
