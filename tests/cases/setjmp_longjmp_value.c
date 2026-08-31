/* SPDX-License-Identifier: MIT */
/* expect: 42 */
#include <setjmp.h>

int main(void)
{
    jmp_buf env;
    int result;

    result = setjmp(env);
    if (result == 0)
        longjmp(env, 42);
    return result;
}
