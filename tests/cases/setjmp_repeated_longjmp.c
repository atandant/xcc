/* SPDX-License-Identifier: MIT */
/* expect: 33 */
#include <setjmp.h>

int main(void)
{
    jmp_buf env;
    volatile int count = 0;
    int result;

    result = setjmp(env);
    if (count < 3) {
        count++;
        longjmp(env, count);
    }
    return count * 10 + result;
}
