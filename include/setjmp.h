/* SPDX-License-Identifier: MIT */
#ifndef __XCC_SETJMP_H
#define __XCC_SETJMP_H

/* Layout matches glibc x86-64 so setjmp/longjmp link against system libc.
   xcc recognizes direct calls to _setjmp/longjmp for codegen; see
   include/README.md ("setjmp / longjmp"). */

typedef long int __jmp_buf[8];

#define _SIGSET_NWORDS (1024 / (8 * sizeof(unsigned long int)))

struct __sigset_t {
    unsigned long int __val[_SIGSET_NWORDS];
};
typedef struct __sigset_t __sigset_t;

struct __jmp_buf_tag {
    __jmp_buf __jmpbuf;
    int __mask_was_saved;
    __sigset_t __saved_mask;
};

typedef struct __jmp_buf_tag jmp_buf[1];

int _setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

#define setjmp(env) _setjmp(env)

#endif
