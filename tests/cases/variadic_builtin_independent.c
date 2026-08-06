/* SPDX-License-Identifier: MIT */
/* expect: 0 */
typedef struct __va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];

int independent(int n, ...)
{
    va_list a;
    va_list b;
    int bad;
    __builtin_va_start(a, n);
    __builtin_va_start(b, n);
    bad = __builtin_va_arg(a, int) != 3;
    bad += __builtin_va_arg(a, int) != 5;
    bad += __builtin_va_arg(b, int) != 3;
    __builtin_va_end(a);
    __builtin_va_end(b);
    return bad;
}

int main(void) { return independent(2, 3, 5); }
