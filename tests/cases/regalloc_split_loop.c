/* SPDX-License-Identifier: MIT */
/* expect: 84 */
int pressure(int a, int b, int c, int d, int e, int f,
             int g, int h, int i, int j, int k, int l)
{
    int count;
    int total;
    count = 0;
    total = 0;
    while (count < 3) {
        total = total + a + b;
        count = count + 1;
    }
    return total + c + d + e + f + g + h + i + j + k + l;
}

int main(void)
{
    return pressure(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12);
}
