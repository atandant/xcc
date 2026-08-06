/* SPDX-License-Identifier: MIT */
#include <stdarg.h>

int check_promoted_ints(int n, ...)
{
    va_list ap; int bad;
    va_start(ap, n);
    bad = va_arg(ap, int) != -4;
    bad += va_arg(ap, int) != 300;
    bad += va_arg(ap, int) != 250;
    va_end(ap); return bad;
}

int check_promoted_float(int n, ...)
{
    va_list ap; int bad;
    va_start(ap, n); bad = va_arg(ap, double) != 1.25;
    bad += va_arg(ap, double) != 9.5; va_end(ap); return bad;
}

int check_unsigned(int n, ...)
{
    va_list ap; int bad;
    va_start(ap, n); bad = va_arg(ap, unsigned int) != 4000000000U;
    bad += va_arg(ap, unsigned long) != 9000000000UL;
    va_end(ap); return bad;
}

int check_pointer(int n, ...)
{
    va_list ap; int *p;
    va_start(ap, n); p = va_arg(ap, int *); va_end(ap); return *p != 91;
}

int check_gpr_boundary(int a, int b, int c, int d, int e, ...)
{
    va_list ap; int bad;
    va_start(ap, e); bad = va_arg(ap, long) != 600L;
    bad += va_arg(ap, long) != 700L; va_end(ap);
    return bad + (a+b+c+d+e != 15);
}

int check_gpr_stack(int a, int b, int c, int d, int e, int f, ...)
{
    va_list ap; int bad;
    va_start(ap, f); bad = va_arg(ap, long) != 700L;
    bad += va_arg(ap, long) != 800L; va_end(ap);
    return bad + (a+b+c+d+e+f != 21);
}

int check_sse_boundary(double a,double b,double c,double d,double e,double f,double g,...)
{
    va_list ap; int bad;
    va_start(ap, g); bad = va_arg(ap, double) != 8.25;
    bad += va_arg(ap, double) != 9.5; va_end(ap);
    return bad + (a+b+c+d+e+f+g != 28.0);
}

int check_sse_stack(double a,double b,double c,double d,double e,double f,double g,double h,...)
{
    va_list ap; int bad;
    va_start(ap, h); bad = va_arg(ap, double) != 9.25;
    bad += va_arg(ap, double) != 10.5; va_end(ap);
    return bad + (a+b+c+d+e+f+g+h != 36.0);
}

int check_mixed(int a, double b, int c, double d, ...)
{
    va_list ap; int bad;
    va_start(ap, d); bad = va_arg(ap, long) != 5L;
    bad += va_arg(ap, double) != 6.5;
    bad += va_arg(ap, long) != 7L;
    bad += va_arg(ap, double) != 8.5;
    va_end(ap); return bad + (a != 1 || b != 2.5 || c != 3 || d != 4.5);
}

int check_indirect(int n, ...)
{
    va_list ap; int bad;
    va_start(ap, n); bad = va_arg(ap, long) != 111L;
    bad += va_arg(ap, double) != 222.5; va_end(ap); return bad;
}

struct One { char x; };
struct Eight { long x; };
struct Pair { long a; long b; };
struct Big { long a; long b; long c; };
union Word { long x; char bytes[8]; };

int check_f80(int n, ...)
{
    va_list ap; long double x;
    va_start(ap, n); x = va_arg(ap, long double); va_end(ap);
    return x != 1.25L;
}

int check_f80_mixed(int n, ...)
{
    va_list ap; int bad;
    va_start(ap, n); bad = va_arg(ap, long) != 2L;
    bad += va_arg(ap, long double) != 3.5L;
    bad += va_arg(ap, double) != 4.25; va_end(ap); return bad;
}

int check_one(int n, ...)
{
    va_list ap; struct One x;
    va_start(ap, n); x = va_arg(ap, struct One); va_end(ap); return x.x != 11;
}

int check_eight(int n, ...)
{
    va_list ap; struct Eight x;
    va_start(ap, n); x = va_arg(ap, struct Eight); va_end(ap); return x.x != 12;
}

int check_pair(int n, ...)
{
    va_list ap; struct Pair x;
    va_start(ap, n); x = va_arg(ap, struct Pair); va_end(ap);
    return x.a != 13 || x.b != 14;
}

int check_big(int n, ...)
{
    va_list ap; struct Big x;
    va_start(ap, n); x = va_arg(ap, struct Big); va_end(ap);
    return x.a != 15 || x.b != 16 || x.c != 17;
}

int check_pair_edge(int a, int b, int c, int d, ...)
{
    va_list ap; struct Pair x;
    va_start(ap, d); x = va_arg(ap, struct Pair); va_end(ap);
    return x.a != 18 || x.b != 19 || a+b+c+d != 10;
}

int check_pair_stack(int a, int b, int c, int d, int e, ...)
{
    va_list ap; struct Pair x;
    va_start(ap, e); x = va_arg(ap, struct Pair); va_end(ap);
    return x.a != 20 || x.b != 21 || a+b+c+d+e != 15;
}

int check_big_then_int(int n, ...)
{
    va_list ap; struct Big x; int y;
    va_start(ap, n); x = va_arg(ap, struct Big); y = va_arg(ap, int); va_end(ap);
    return x.a != 22 || x.b != 23 || x.c != 24 || y != 26;
}

int check_union(int n, ...)
{
    va_list ap; union Word x;
    va_start(ap, n); x = va_arg(ap, union Word); va_end(ap); return x.x != 25;
}
