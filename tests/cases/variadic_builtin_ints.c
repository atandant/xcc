/* SPDX-License-Identifier: MIT */
/* expect: 10 */
typedef struct __va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];

int sum(int n, ...)
{
    va_list ap;
    int i;
    int total = 0;
    __builtin_va_start(ap, n);
    for (i = 0; i < n; i++)
        total += __builtin_va_arg(ap, int);
    __builtin_va_end(ap);
    return total;
}

int main(void) { return sum(4, (char)1, (short)2, 3, 4); }
