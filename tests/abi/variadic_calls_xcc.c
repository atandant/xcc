/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* abi-role: caller */
/* abi-peer: variadic_calls_gcc.c */
int check_promoted_ints(int, ...);
int check_promoted_float(int, ...);
int check_unsigned(int, ...);
int check_pointer(int, ...);
int check_gpr_boundary(int, int, int, int, int, ...);
int check_gpr_stack(int, int, int, int, int, int, ...);
int check_sse_boundary(double, double, double, double, double, double, double, ...);
int check_sse_stack(double, double, double, double, double, double, double, double, ...);
int check_mixed(int, double, int, double, ...);
int check_indirect(int, ...);
struct One { char x; };
struct Eight { long x; };
struct Pair { long a; long b; };
struct Big { long a; long b; long c; };
union Word { long x; char bytes[8]; };
int check_f80(int, ...);
int check_f80_mixed(int, ...);
int check_one(int, ...);
int check_eight(int, ...);
int check_pair(int, ...);
int check_big(int, ...);
int check_pair_edge(int, int, int, int, ...);
int check_pair_stack(int, int, int, int, int, ...);
int check_big_then_int(int, ...);
int check_union(int, ...);

int main(void)
{
    int x = 91;
    int (*fn)(int, ...) = check_indirect;
    int bad = 0;
    struct One one;
    struct Eight eight;
    struct Pair pair;
    struct Big big;
    union Word word;

    bad = bad * 2 + !!check_promoted_ints(3, (char)-4, (short)300, (unsigned char)250);
    bad = bad * 2 + !!check_promoted_float(2, (float)1.25, 9.5);
    bad = bad * 2 + !!check_unsigned(2, (unsigned int)4000000000U, (unsigned long)9000000000UL);
    bad = bad * 2 + !!check_pointer(1, &x);
    bad = bad * 2 + !!check_gpr_boundary(1, 2, 3, 4, 5, 600L, 700L);
    bad = bad * 2 + !!check_gpr_stack(1, 2, 3, 4, 5, 6, 700L, 800L);
    bad = bad * 2 + !!check_sse_boundary(1, 2, 3, 4, 5, 6, 7, 8.25, 9.5);
    bad = bad * 2 + !!check_sse_stack(1, 2, 3, 4, 5, 6, 7, 8, 9.25, 10.5);
    bad = bad * 2 + !!check_mixed(1, 2.5, 3, 4.5, 5L, 6.5, 7L, 8.5);
    bad = bad * 2 + !!fn(2, 111L, 222.5);
    one.x = 11;
    eight.x = 12;
    pair.a = 13;
    pair.b = 14;
    big.a = 15;
    big.b = 16;
    big.c = 17;
    word.x = 25;
    bad = bad * 2 + !!check_f80(1, 1.25L);
    bad = bad * 2 + !!check_f80_mixed(3, 2L, 3.5L, 4.25);
    bad = bad * 2 + !!check_one(1, one);
    bad = bad * 2 + !!check_eight(1, eight);
    bad = bad * 2 + !!check_pair(1, pair);
    bad = bad * 2 + !!check_big(1, big);
    pair.a = 18; pair.b = 19;
    bad = bad * 2 + !!check_pair_edge(1, 2, 3, 4, pair);
    pair.a = 20; pair.b = 21;
    bad = bad * 2 + !!check_pair_stack(1, 2, 3, 4, 5, pair);
    big.a = 22; big.b = 23; big.c = 24;
    bad = bad * 2 + !!check_big_then_int(1, big, 26);
    bad = bad * 2 + !!check_union(1, word);
    return bad;
}
