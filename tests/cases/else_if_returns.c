/* SPDX-License-Identifier: MIT */
/* expect: 30 */
int score(int x)
{
    if (x < 10)
        return 10;
    else if (x < 20)
        return 20;
    else if (x < 30)
        return 30;
    else
        return 40;
}

int main(void)
{
    return score(25);
}
