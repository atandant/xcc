/* SPDX-License-Identifier: MIT */
int xcc_va_ints(int, ...);
int xcc_va_unsigned(int, ...);
int xcc_va_pointer(int, ...);
int xcc_va_double(int, ...);
int xcc_va_gpr_edge(int,int,int,int,int,...);
int xcc_va_gpr_stack(int,int,int,int,int,int,...);
int xcc_va_sse_edge(double,double,double,double,double,double,double,...);
int xcc_va_sse_stack(double,double,double,double,double,double,double,double,...);
int xcc_va_mixed(int,double,...);
int xcc_va_forward(int,...);
struct One { char x; }; struct Eight { long x; };
struct Pair { long a; long b; }; struct Big { long a; long b; long c; };
union Word { long x; char bytes[8]; };
int xcc_va_f80(int,...); int xcc_va_f80_named(long double,int,...);
int xcc_va_f80_mixed(int,...); int xcc_va_one(int,...); int xcc_va_eight(int,...);
int xcc_va_pair(int,...); int xcc_va_big(int,...);
int xcc_va_pair_edge(int,int,int,int,...);
int xcc_va_pair_stack(int,int,int,int,int,...);
int xcc_va_big_then_int(int,...); int xcc_va_record_sequence(int,...);
int xcc_va_union(int,...); int xcc_va_independent(int,...); int xcc_va_restart(int,...);
struct Big xcc_va_sret(int,...);
int xcc_va_named_stack(int,int,int,int,int,int,struct Big,...);
int xcc_va_format(char *,int,...); int xcc_va_indirect_target(int,...);

int main(void)
{
    int x = 77;
    int bad = 0;
    bad = bad * 2 + !!xcc_va_ints(3, (char)-9, (short)300, (unsigned char)240);
    bad = bad * 2 + !!xcc_va_unsigned(2, 4000000000U, 9000000000UL);
    bad = bad * 2 + !!xcc_va_pointer(1, &x);
    bad = bad * 2 + !!xcc_va_double(2, (float)1.25, 8.5);
    bad = bad * 2 + !!xcc_va_gpr_edge(1,2,3,4,5,61L,62L);
    bad = bad * 2 + !!xcc_va_gpr_stack(1,2,3,4,5,6,71L,72L);
    bad = bad * 2 + !!xcc_va_sse_edge(1,2,3,4,5,6,7,8.25,9.5);
    bad = bad * 2 + !!xcc_va_sse_stack(1,2,3,4,5,6,7,8,9.25,10.5);
    bad = bad * 2 + !!xcc_va_mixed(1,2.5,3L,4.5,5L,6.5);
    bad = bad * 2 + !!xcc_va_forward(3,81L,82.5,83L);
    {
        struct One one; struct Eight eight; struct Pair pair; struct Big big;
        union Word word; struct Big returned; char out[16];
        int (*indirect)(int,...) = xcc_va_indirect_target;
        one.x=11; eight.x=12; pair.a=13; pair.b=14;
        big.a=15; big.b=16; big.c=17; word.x=29;
        bad = bad * 2 + !!xcc_va_f80(1,1.25L);
        bad = bad * 2 + !!xcc_va_f80_named(2.5L,1,3.75L);
        bad = bad * 2 + !!xcc_va_f80_mixed(3,4L,5.5L,6.25);
        bad = bad * 2 + !!xcc_va_one(1,one);
        bad = bad * 2 + !!xcc_va_eight(1,eight);
        bad = bad * 2 + !!xcc_va_pair(1,pair);
        bad = bad * 2 + !!xcc_va_big(1,big);
        pair.a=18; pair.b=19; bad = bad * 2 + !!xcc_va_pair_edge(1,2,3,4,pair);
        pair.a=20; pair.b=21; bad = bad * 2 + !!xcc_va_pair_stack(1,2,3,4,5,pair);
        big.a=22; big.b=23; big.c=24; bad = bad * 2 + !!xcc_va_big_then_int(1,big,25);
        one.x=26; pair.a=27; pair.b=28; bad = bad * 2 + !!xcc_va_record_sequence(2,one,pair);
        bad = bad * 2 + !!xcc_va_union(1,word);
        bad = bad * 2 + !!xcc_va_independent(2,30,31);
        bad = bad * 2 + !!xcc_va_restart(1,32);
        returned=xcc_va_sret(3,33L,34L,35L); bad = bad * 2 + !!(returned.a!=33||returned.b!=34||returned.c!=35);
        big.a=36; big.b=37; big.c=38; pair.a=39; pair.b=40;
        bad = bad * 2 + !!xcc_va_named_stack(1,2,3,4,5,6,big,pair);
        bad = bad * 2 + !!(xcc_va_format(out,2,41,4.5)||out[0]!='4'||out[1]!='1'||out[2]!=' '||out[3]!='4'||out[4]!='.'||out[5]!='5'||out[6]!=0);
        bad = bad * 2 + !!indirect(1,42);
    }
    return bad;
}
