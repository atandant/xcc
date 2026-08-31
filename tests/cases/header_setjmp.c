/* SPDX-License-Identifier: MIT */
/* expect: 99 */
#include <setjmp.h>

int main(void)
{
    jmp_buf env;
    volatile int step;

    step = 0;
    if (setjmp(env) == 0) {
        step = 1;
        longjmp(env, 99);
    }
    if (step != 1)
        return 1;
    return 99;
}
