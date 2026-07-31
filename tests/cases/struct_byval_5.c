/* SPDX-License-Identifier: MIT */
/* expect: 15 */
struct S5 { char a; char b; char c; char d; char e; };

struct S5 make5(void)
{
    struct S5 s;
    s.a = 1;
    s.b = 2;
    s.c = 3;
    s.d = 4;
    s.e = 5;
    return s;
}

int sum5(struct S5 s)
{
    return s.a + s.b + s.c + s.d + s.e;
}

int main(void)
{
    return sum5(make5());
}
