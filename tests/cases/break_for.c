/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int sum_until(int n, int stop)
{
    int i;
    int total;

    total = 0;
    for (i = 0; i < n; i = i + 1) {
        if (i == stop)
            break;
        total = total + i;
    }
    return total;
}

int main(void)
{
    return sum_until(10, 4);
}
