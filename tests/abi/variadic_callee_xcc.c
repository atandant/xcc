/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* abi-role: callee */
/* abi-peer: variadic_callee_gcc.c */
typedef struct __va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

int xcc_va_ints(int n, ...)
{
    va_list ap; int bad; va_start(ap, n);
    bad = va_arg(ap, int) != -9; bad += va_arg(ap, int) != 300;
    bad += va_arg(ap, int) != 240; va_end(ap); return bad;
}
int xcc_va_unsigned(int n, ...)
{
    va_list ap; int bad; va_start(ap, n);
    bad = va_arg(ap, unsigned int) != 4000000000U;
    bad += va_arg(ap, unsigned long) != 9000000000UL; va_end(ap); return bad;
}
int xcc_va_pointer(int n, ...)
{
    va_list ap; int *p; va_start(ap, n); p = va_arg(ap, int *);
    va_end(ap); return *p != 77;
}
int xcc_va_double(int n, ...)
{
    va_list ap; int bad; va_start(ap, n);
    bad = va_arg(ap, double) != 1.25; bad += va_arg(ap, double) != 8.5;
    va_end(ap); return bad;
}
int xcc_va_gpr_edge(int a,int b,int c,int d,int e,...)
{
    va_list ap; int bad; va_start(ap, e);
    bad = va_arg(ap, long) != 61L; bad += va_arg(ap, long) != 62L;
    va_end(ap); return bad + (a+b+c+d+e != 15);
}
int xcc_va_gpr_stack(int a,int b,int c,int d,int e,int f,...)
{
    va_list ap; int bad; va_start(ap, f);
    bad = va_arg(ap, long) != 71L; bad += va_arg(ap, long) != 72L;
    va_end(ap); return bad + (a+b+c+d+e+f != 21);
}
int xcc_va_sse_edge(double a,double b,double c,double d,double e,double f,double g,...)
{
    va_list ap; int bad; va_start(ap, g);
    bad = va_arg(ap, double) != 8.25; bad += va_arg(ap, double) != 9.5;
    va_end(ap); return bad + (a+b+c+d+e+f+g != 28.0);
}
int xcc_va_sse_stack(double a,double b,double c,double d,double e,double f,double g,double h,...)
{
    va_list ap; int bad; va_start(ap, h);
    bad = va_arg(ap, double) != 9.25; bad += va_arg(ap, double) != 10.5;
    va_end(ap); return bad + (a+b+c+d+e+f+g+h != 36.0);
}
int xcc_va_mixed(int a,double b,...)
{
    va_list ap; int bad; va_start(ap, b);
    bad = va_arg(ap, long) != 3L; bad += va_arg(ap, double) != 4.5;
    bad += va_arg(ap, long) != 5L; bad += va_arg(ap, double) != 6.5;
    va_end(ap); return bad + (a != 1 || b != 2.5);
}
static int consume_two(va_list ap)
{
    int bad = va_arg(ap, long) != 81L;
    return bad + (va_arg(ap, double) != 82.5);
}
int xcc_va_forward(int n,...)
{
    va_list ap; int bad; va_start(ap, n); bad = consume_two(ap);
    bad += va_arg(ap, long) != 83L; va_end(ap); return bad;
}

struct One { char x; };
struct Eight { long x; };
struct Pair { long a; long b; };
struct Big { long a; long b; long c; };
union Word { long x; char bytes[8]; };

int xcc_va_f80(int n,...)
{ va_list ap; long double x; va_start(ap,n); x=va_arg(ap,long double); va_end(ap); return x!=1.25L; }
int xcc_va_f80_named(long double fixed,int n,...)
{ va_list ap; long double x; va_start(ap,n); x=va_arg(ap,long double); va_end(ap); return fixed!=2.5L||x!=3.75L; }
int xcc_va_f80_mixed(int n,...)
{ va_list ap; int bad; va_start(ap,n); bad=va_arg(ap,long)!=4L; bad+=va_arg(ap,long double)!=5.5L; bad+=va_arg(ap,double)!=6.25; va_end(ap); return bad; }
int xcc_va_one(int n,...)
{ va_list ap; struct One x; va_start(ap,n); x=va_arg(ap,struct One); va_end(ap); return x.x!=11; }
int xcc_va_eight(int n,...)
{ va_list ap; struct Eight x; va_start(ap,n); x=va_arg(ap,struct Eight); va_end(ap); return x.x!=12; }
int xcc_va_pair(int n,...)
{ va_list ap; struct Pair x; va_start(ap,n); x=va_arg(ap,struct Pair); va_end(ap); return x.a!=13||x.b!=14; }
int xcc_va_big(int n,...)
{ va_list ap; struct Big x; va_start(ap,n); x=va_arg(ap,struct Big); va_end(ap); return x.a!=15||x.c!=17; }
int xcc_va_pair_edge(int a,int b,int c,int d,...)
{ va_list ap; struct Pair x; va_start(ap,d); x=va_arg(ap,struct Pair); va_end(ap); return x.a!=18||x.b!=19||a+b+c+d!=10; }
int xcc_va_pair_stack(int a,int b,int c,int d,int e,...)
{ va_list ap; struct Pair x; va_start(ap,e); x=va_arg(ap,struct Pair); va_end(ap); return x.a!=20||x.b!=21||a+b+c+d+e!=15; }
int xcc_va_big_then_int(int n,...)
{ va_list ap; struct Big x; int y; va_start(ap,n); x=va_arg(ap,struct Big); y=va_arg(ap,int); va_end(ap); return x.a!=22||x.c!=24||y!=25; }
int xcc_va_record_sequence(int n,...)
{ va_list ap; struct One a; struct Pair b; va_start(ap,n); a=va_arg(ap,struct One); b=va_arg(ap,struct Pair); va_end(ap); return a.x!=26||b.a!=27||b.b!=28; }
int xcc_va_union(int n,...)
{ va_list ap; union Word x; va_start(ap,n); x=va_arg(ap,union Word); va_end(ap); return x.x!=29; }
int xcc_va_independent(int n,...)
{ va_list a; va_list b; int bad; va_start(a,n); va_start(b,n); bad=va_arg(a,int)!=30; bad+=va_arg(a,int)!=31; bad+=va_arg(b,int)!=30; va_end(a); va_end(b); return bad; }
int xcc_va_restart(int n,...)
{ va_list ap; int bad; va_start(ap,n); bad=va_arg(ap,int)!=32; va_end(ap); va_start(ap,n); bad+=va_arg(ap,int)!=32; va_end(ap); return bad; }
struct Big xcc_va_sret(int n,...)
{ va_list ap; struct Big x; va_start(ap,n); x.a=va_arg(ap,long); x.b=va_arg(ap,long); x.c=va_arg(ap,long); va_end(ap); return x; }
int xcc_va_named_stack(int a,int b,int c,int d,int e,int f,struct Big fixed,...)
{ va_list ap; struct Pair x; va_start(ap,fixed); x=va_arg(ap,struct Pair); va_end(ap); return fixed.a!=36||fixed.c!=38||x.a!=39||x.b!=40||a+b+c+d+e+f!=21; }
int vsprintf(char *, const char *, void *);
int xcc_va_format(char *out,int n,...)
{ va_list ap; int r; va_start(ap,n); r=vsprintf(out,"%d %.1f",ap); va_end(ap); return r!=6; }
int xcc_va_indirect_target(int n,...)
{ va_list ap; int x; va_start(ap,n); x=va_arg(ap,int); va_end(ap); return x!=42; }
