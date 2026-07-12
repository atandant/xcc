/* SPDX-License-Identifier: MIT */
/* expect: 158 */
/* abi-role: caller */
/* abi-peer: xcc_caller_gcc.c */
struct Pair { long a; long b; };
struct Nine { char bytes[9]; };
struct Big { long a; long b; long c; };

int pair_after_five(int, int, int, int, int, struct Pair);
int nine_after_five(int, int, int, int, int, struct Nine);
int big_then_int(struct Big, int);
struct Pair make_pair(long, long);
struct Big make_big(long, long, long);
struct Big sret_pair_boundary(int, int, int, int, struct Pair);

int main(void)
{
    struct Pair pair;
    struct Pair returned_pair;
    struct Nine nine;
    struct Big big;
    struct Big boundary;
    int sum;

    pair.a = 6;
    pair.b = 7;
    nine.bytes[0] = 8;
    nine.bytes[8] = 9;

    sum = pair_after_five(1, 2, 3, 4, 5, pair);
    sum = sum + nine_after_five(1, 2, 3, 4, 5, nine);
    big = make_big(10, 11, 12);
    sum = sum + big_then_int(big, 13);
    returned_pair = make_pair(14, 15);
    sum = sum + (int)returned_pair.a + (int)returned_pair.b;
    boundary = sret_pair_boundary(1, 2, 3, 4, pair);
    sum = sum + (int)boundary.a + (int)boundary.b + (int)boundary.c;
    return sum;
}
