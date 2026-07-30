/* SPDX-License-Identifier: MIT */
/* expect: 57 */
int side(int x) { return x + 1; }

int pressure(int (*fn)(int), int a, int b, int c, int d, int e,
             int f, int g, int h, int i, int j)
{
    int x;
    x = fn(a);
    return x + a + b + c + d + e + f + g + h + i + j;
}

int main(void)
{
    return pressure(side, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}
