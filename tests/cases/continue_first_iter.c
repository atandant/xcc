/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int skip_zero(int n)
{
    int i;
    int total;

    total = 0;
    for (i = 0; i < n; i = i + 1) {
        if (i == 0)
            continue;
        total = total + i;
    }
    return total;
}

int main(void)
{
    return skip_zero(4);
}
