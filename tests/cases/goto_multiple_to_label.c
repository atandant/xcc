/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int choose(int value)
{
    if (value == 1)
        goto selected;
    if (value == 2)
        goto selected;
    return 0;
selected:
    return value + 6;
}

int main(void)
{
    return choose(1) + choose(2) - 6;
}
