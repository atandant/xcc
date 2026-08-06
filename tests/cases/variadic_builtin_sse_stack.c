/* SPDX-License-Identifier: MIT */
/* expect: 0 */
typedef struct __va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];

int sse_stack(double a, double b, double c, double d,
              double e, double f, double g, double h, ...)
{
    va_list ap;
    double x;
    __builtin_va_start(ap, h);
    x = __builtin_va_arg(ap, double);
    __builtin_va_end(ap);
    return a+b+c+d+e+f+g+h != 36.0 || x != 9.5;
}

int main(void) { return sse_stack(1, 2, 3, 4, 5, 6, 7, 8, 9.5); }
