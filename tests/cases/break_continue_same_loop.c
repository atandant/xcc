/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int break_after_skip(int n)
{
    int i;
    int total;

    total = 0;
    for (i = 0; i < n; i = i + 1) {
        if (i == 2)
            continue;
        if (i == 4)
            break;
        total = total + 1;
    }
    return total;
}

int main(void)
{
    return break_after_skip(10);
}
