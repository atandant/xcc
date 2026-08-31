/* SPDX-License-Identifier: MIT */
/* expect: 27 */
#include <setjmp.h>

int main(void)
{
    jmp_buf env;
    int result;

    result = setjmp(env);
    if (result == 0)
        (longjmp)(env, 27);
    return result;
}
