/* SPDX-License-Identifier: MIT */
/* expect: 99 */
/* Struct >16 bytes returned via hidden sret pointer (lower-owned). */
struct Big { long a; long b; long c; };

struct Big make(void) {
    struct Big b;
    b.a = 10;
    b.b = 20;
    b.c = 69;
    return b;
}

int main(void) {
    struct Big x = make();
    return (int)(x.a + x.b + x.c);   /* 10+20+69 = 99 */
}
