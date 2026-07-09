/* SPDX-License-Identifier: MIT */
/* expect: 5 */
/* Chained large struct return via sret forwarding. */
struct Big { long a; long b; long c; };

struct Big inner(void) {
    struct Big b;
    b.a = 1;
    b.b = 2;
    b.c = 2;
    return b;
}

struct Big outer(void) {
    return inner();
}

int main(void) {
    struct Big x = outer();
    return (int)(x.a + x.b + x.c);   /* 5 */
}
