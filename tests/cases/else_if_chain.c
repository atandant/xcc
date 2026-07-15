/* SPDX-License-Identifier: MIT */
/* expect: 2 */
int classify(int x)
{
    if (x < 0)
        return 1;
    else if (x == 0)
        return 2;
    else
        return 3;
}

int main(void)
{
    return classify(0);
}
