/* SPDX-License-Identifier: MIT */
/* expect: 56 */
int side(int x) { return x + 1; }

int pressure(int *values, int index, int a, int b, int c,
             int d, int e, int f, int g, int h, int i, int j)
{
    int x;
    values[index] = a + b;
    x = side(c);
    return values[index] + x + d + e + f + g + h + i + j;
}

int main(void)
{
    int values[2];
    return pressure(values, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
}
