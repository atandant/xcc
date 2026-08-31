/* SPDX-License-Identifier: MIT */
/* expect: 23 */
#include <setjmp.h>

int main(void)
{
    jmp_buf env;
    volatile int value = 5;

    if (setjmp(env) == 0) {
        value = 23;
        longjmp(env, 1);
    }
    return value;
}
