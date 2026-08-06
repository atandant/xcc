/* SPDX-License-Identifier: MIT */
/* expect: 11 */
#include <errno.h>

int main(void)
{
    if (EDOM == 0 || ERANGE == 0)
        return 1;
    if (EINVAL == 0 || ENOENT == 0)
        return 2;
    errno = 7;
    if (errno != 7)
        return 3;
    return 11;
}
