/* SPDX-License-Identifier: MIT */
/* expect: 7 */
#include <setjmp.h>

int main(void)
{
    jmp_buf env;

    if (setjmp(env) != 0)
        return 1;
    return 7;
}
