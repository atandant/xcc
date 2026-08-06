/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <string.h>

int main(void)
{
    if (strcmp("abc", "abc") != 0)
        return 1;
    if (strcmp("ab", "ac") >= 0)
        return 2;
    if (strncmp("abc", "abd", 2) != 0)
        return 3;
    if (strlen("abc") != 3)
        return 4;
    return 0;
}
