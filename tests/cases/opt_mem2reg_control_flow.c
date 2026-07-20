/* SPDX-License-Identifier: MIT */
/* expect: 117 */

int bump(int x)
{
    return x + 1;
}

int diamond(int flag)
{
    int x;

    x = 3;
    if (flag)
        x = 11;
    else
        x = 7;
    return x;
}

int loops(int n)
{
    int i;
    int j;
    int sum;

    i = 0;
    sum = 0;
    while (i < n) {
        j = 0;
        while (j < 4) {
            j = j + 1;
            if (j == 2)
                continue;
            sum = sum + i + j;
        }
        i = bump(i);
    }
    return sum;
}

int address_taken(void)
{
    int x;
    int *p;

    x = 2;
    p = &x;
    *p = *p + 5;
    return x;
}

int narrow(void)
{
    char c;
    short s;

    c = 300;
    s = 65538;
    return c + s;
}

int nested_scope(int flag)
{
    int result;

    result = 0;
    if (flag) {
        int inner;
        inner = 13;
        result = inner;
    }
    return result;
}

int main(void)
{
    return diamond(1) + diamond(0) + loops(3) +
           address_taken() + narrow() + nested_scope(1);
}
