/* SPDX-License-Identifier: MIT */
#ifndef __XCC_STDARG_H
#define __XCC_STDARG_H

/* System V AMD64 va_list layout. */
typedef struct __xcc_va_list_tag {
    unsigned int gp_offset;
    unsigned int fp_offset;
    void *overflow_arg_area;
    void *reg_save_area;
} va_list[1];

#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

#endif
