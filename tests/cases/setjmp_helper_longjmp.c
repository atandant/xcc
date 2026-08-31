/* SPDX-License-Identifier: MIT */
/* expect: 19 */
#include <setjmp.h>

void jump_from_helper(jmp_buf env)
{
    longjmp(env, 19);
}

int main(void)
{
    jmp_buf env;
    int result;

    result = setjmp(env);
    if (result == 0)
        jump_from_helper(env);
    return result;
}
