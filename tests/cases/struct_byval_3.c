/* SPDX-License-Identifier: MIT */
/* expect: 33 */
struct S3 { char a; char b; char c; };

struct S3 make3(int n)
{
    struct S3 s;
    s.a = n;
    s.b = n + 1;
    s.c = n + 2;
    return s;
}

int sum3(struct S3 s)
{
    return s.a + s.b + s.c;
}

int main(void)
{
    return sum3(make3(10));
}
