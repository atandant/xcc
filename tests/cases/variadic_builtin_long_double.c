/* SPDX-License-Identifier: MIT */
/* expect: 0 */
typedef struct __va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];

int check_long_double(int n, ...)
{
    va_list ap;
    long double x;
    __builtin_va_start(ap, n);
    x = __builtin_va_arg(ap, long double);
    __builtin_va_end(ap);
    return x != 12.5L;
}

int main(void) { return check_long_double(1, 12.5L); }
