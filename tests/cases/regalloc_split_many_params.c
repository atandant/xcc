/* SPDX-License-Identifier: MIT */
/* expect: 107 */
int side(int x) { return x + 1; }

int pressure(int a, int b, int c, int d, int e, int f, int g,
             int h, int i, int j, int k, int l, int m, int n)
{
    int x;
    x = side(a);
    return x + a + b + c + d + e + f + g + h + i + j + k + l + m + n;
}

int main(void)
{
    return pressure(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14);
}
