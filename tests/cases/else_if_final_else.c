/* SPDX-License-Identifier: MIT */
/* expect: 99 */
int fallback(int x)
{
    if (x == 1)
        return 10;
    else if (x == 2)
        return 20;
    else if (x == 3)
        return 30;
    else
        return 99;
}

int main(void)
{
    return fallback(7);
}
