/* SPDX-License-Identifier: MIT */
/* expect: 7 */
typedef struct __va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];

int mixed(int n, ...)
{
    va_list ap;
    long a;
    double b;
    int c;
    __builtin_va_start(ap, n);
    a = __builtin_va_arg(ap, long);
    b = __builtin_va_arg(ap, double);
    c = __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    return a + (int)b + c;
}

int main(void) { return mixed(3, 1L, (float)2.5, 4); }
