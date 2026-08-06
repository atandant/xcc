/* SPDX-License-Identifier: MIT */
/* expect: 15 */
typedef struct __va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];

int stack_args(int a, int b, int c, int d, int e, int f, ...)
{
    va_list ap;
    int x;
    int y;
    __builtin_va_start(ap, f);
    x = __builtin_va_arg(ap, int);
    y = __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    return x + y;
}

int main(void) { return stack_args(1, 2, 3, 4, 5, 6, 7, 8); }
