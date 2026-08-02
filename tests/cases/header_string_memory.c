/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* xcc-args: -Iinclude */
#include <string.h>

int main(void)
{
    char dst[8];
    char src[4] = { 'a', 'b', 'c', '\0' };

    memcpy(dst, src, 4);
    if (memcmp(dst, src, 4) != 0)
        return 1;
    memset(dst, 'x', 3);
    dst[3] = '\0';
    if (dst[0] != 'x' || dst[2] != 'x')
        return 2;
    if (memchr(dst, 'x', 4) != dst)
        return 3;
    return 0;
}
