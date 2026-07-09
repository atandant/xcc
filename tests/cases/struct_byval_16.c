/* SPDX-License-Identifier: MIT */
/* expect: 9 */
/* 9–16 byte struct uses two GPRs for pass and return. */
struct P { int a; int b; int c; };

struct P add(struct P p) {
    struct P r;
    r.a = p.a + 1;
    r.b = p.b + 1;
    r.c = p.c + 1;
    return r;
}

int main(void) {
    struct P in;
    struct P out;
    in.a = 1;
    in.b = 2;
    in.c = 3;
    out = add(in);
    return out.a + out.b + out.c;   /* 2+3+4 = 9? 1+1=2, 2+1=3, 3+1=4 -> 9 */
}
