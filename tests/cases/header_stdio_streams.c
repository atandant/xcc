/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdio.h>

int main(void)
{
    if (!stdin || !stdout || !stderr)
        return 1;
    if (fprintf(stdout, "") != 0 || fprintf(stderr, "") != 0)
        return 2;
    if (fflush(stdout) != 0 || fflush(stderr) != 0)
        return 3;
    return 0;
}
