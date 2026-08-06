/* SPDX-License-Identifier: MIT */
/* expect: 6 */
typedef struct __va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];

static int take_one(va_list ap)
{
    return __builtin_va_arg(ap, int);
}

int forward(int n, ...)
{
    va_list ap;
    int a;
    int b;
    __builtin_va_start(ap, n);
    a = take_one(ap);
    b = __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    return a + b;
}

int main(void) { return forward(2, 2, 4); }
