/* SPDX-License-Identifier: MIT */
/* expect: 42 */
#include <stdlib.h>

int main(void)
{
    char *end;
    long value;

    if (atoi("42") != 42)
        return 1;
    if (atol("-7") != -7)
        return 2;
    value = strtol("2a", &end, 16);
    if (value != 42 || !end || *end != '\0')
        return 3;
    if (strtoul("10", 0, 10) != 10UL)
        return 4;
    return 42;
}
