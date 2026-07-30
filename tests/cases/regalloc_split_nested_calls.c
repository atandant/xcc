/* SPDX-License-Identifier: MIT */
/* expect: 80 */
int side(int x) { return x + 1; }

int pressure(int a, int b, int c, int d, int e, int f,
             int g, int h, int i, int j, int k, int l)
{
    int x;
    int y;
    x = side(a);
    y = side(b);
    return x + y + c + d + e + f + g + h + i + j + k + l;
}

int main(void)
{
    return pressure(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
}
