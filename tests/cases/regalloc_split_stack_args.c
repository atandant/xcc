/* SPDX-License-Identifier: MIT */
/* expect: 120 */
int sum15(int a, int b, int c, int d, int e, int f, int g, int h,
          int i, int j, int k, int l, int m, int n, int o)
{
    return a + b + c + d + e + f + g + h + i + j + k + l + m + n + o;
}

int main(void)
{
    return sum15(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
}
