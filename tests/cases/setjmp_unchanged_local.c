/* SPDX-License-Identifier: MIT */
/* expect: 38 */
#include <setjmp.h>

int main(void)
{
    jmp_buf env;
    int preserved = 37;
    int result;

    result = setjmp(env);
    if (result == 0)
        longjmp(env, 1);
    return preserved + result;
}
