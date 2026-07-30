/* SPDX-License-Identifier: MIT */
/* expect: 31 */
int side(int value) { return value + 1; }

int pressure(int a, int b, int c, int d, int e, int f)
{
    int hot;
    int i;
    int result;

    hot = 0;
    i = 0;
    result = side(a);
    while (i < 4) {
        hot = hot + b;
        i = i + 1;
    }
    return hot + result + a + b + c + d + e + f;
}

int main(void) { return pressure(1, 2, 3, 4, 5, 6); }
