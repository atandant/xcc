/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int first_even(int n)
{
    int i;

    for (i = 0; i < n; i = i + 1) {
        if ((i % 2) != 0)
            continue;
        return i;
    }
    return -1;
}

int main(void)
{
    return first_even(10);
}
