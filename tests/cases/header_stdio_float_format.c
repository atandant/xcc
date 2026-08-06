/* SPDX-License-Identifier: MIT */
/* expect-stdout: 2.5 3.75 */
#include <stdio.h>

int main(void)
{
    printf("%.1f %.2Lf\n", (float)2.5, 3.75L);
    return 0;
}
