/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int bucket(int x)
{
    if (x < 0)
        return 1;
    else if (x == 0)
        return 2;
    else if (x == 1)
        return 3;
    else if (x == 2)
        return 4;
    else
        return 5;
}

int main(void)
{
    return bucket(2);
}
