/* SPDX-License-Identifier: MIT */
/* expect: 0 */
typedef struct __va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];

int restart(int n, ...)
{
    va_list ap;
    int bad;
    __builtin_va_start(ap, n);
    bad = __builtin_va_arg(ap, int) != 9;
    __builtin_va_end(ap);
    __builtin_va_start(ap, n);
    bad += __builtin_va_arg(ap, int) != 9;
    __builtin_va_end(ap);
    return bad;
}

int main(void) { return restart(1, 9); }
