/* SPDX-License-Identifier: MIT */
/* expect: 158 */
/* abi-role: callee */
/* abi-peer: xcc_callee_gcc.c */
struct Pair { long a; long b; };
struct Nine { char bytes[9]; };
struct Big { long a; long b; long c; };

int pair_after_five(int a, int b, int c, int d, int e, struct Pair pair)
{
    return a + b + c + d + e + (int)pair.a + (int)pair.b;
}

int nine_after_five(int a, int b, int c, int d, int e, struct Nine nine)
{
    return a + b + c + d + e + nine.bytes[0] + nine.bytes[8];
}

int big_then_int(struct Big big, int value)
{
    return (int)big.a + (int)big.b + (int)big.c + value;
}

struct Pair make_pair(long a, long b)
{
    struct Pair result;
    result.a = a;
    result.b = b;
    return result;
}

struct Big make_big(long a, long b, long c)
{
    struct Big result;
    result.a = a;
    result.b = b;
    result.c = c;
    return result;
}

struct Big sret_pair_boundary(int a, int b, int c, int d, struct Pair pair)
{
    struct Big result;
    result.a = a + b + c + d;
    result.b = pair.a;
    result.c = pair.b;
    return result;
}
