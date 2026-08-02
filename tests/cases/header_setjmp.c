/* SPDX-License-Identifier: MIT */
/* expect: 99 */
/* xcc-args: -Iinclude */
#include <setjmp.h>

int main(void)
{
    jmp_buf env;
    int step;

    step = 0;
    if (setjmp(env) == 0) {
        step = 1;
        longjmp(env, 99);
    }
    if (step != 1)
        return 1;
    return 99;
}
