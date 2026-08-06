/* SPDX-License-Identifier: MIT */
/* expect: 15 */
typedef struct __va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];
struct Pair { long a; long b; };

int pair_sum(int n, ...)
{
    va_list ap;
    struct Pair p;
    __builtin_va_start(ap, n);
    p = __builtin_va_arg(ap, struct Pair);
    __builtin_va_end(ap);
    return p.a + p.b;
}

int main(void)
{
    struct Pair p;
    p.a = 7;
    p.b = 8;
    return pair_sum(1, p);
}
