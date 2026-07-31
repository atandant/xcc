/* SPDX-License-Identifier: MIT */
/* expect: 28 */
struct S7 { char a[7]; };

struct S7 make7(void)
{
    struct S7 s;
    int i;

    for (i = 0; i < 7; i = i + 1)
        s.a[i] = i + 1;
    return s;
}

int sum7(struct S7 s)
{
    int i;
    int sum;

    sum = 0;
    for (i = 0; i < 7; i = i + 1)
        sum = sum + s.a[i];
    return sum;
}

int main(void)
{
    return sum7(make7());
}
