/* SPDX-License-Identifier: MIT */
/* expect: 59 */
int side(int x) { return x + 1; }

int pressure(int a, int b, int c, int d, int e, int f,
             int g, int h, int i, int j, int k, int l, int choose)
{
    int x;
    x = side(a);
    if (choose)
        return x + a + b + c + d + e + f;
    return x + g + h + i + j + k + l;
}

int main(void)
{
    return pressure(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0);
}
