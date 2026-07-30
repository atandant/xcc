/* SPDX-License-Identifier: MIT */
/* expect: 36 */
int sum_eight(int a, int b, int c, int d,
              int e, int f, int g, int h)
{
    return a + b + c + d + e + f + g + h;
}

int main(void)
{
    int (*fn)(int, int, int, int, int, int, int, int) = sum_eight;
    return fn(1, 2, 3, 4, 5, 6, 7, 8);
}
