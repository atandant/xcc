/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <ctype.h>

int main(void)
{
    if (!isalpha('A') || !islower('a') || islower('A'))
        return 1;
    if (!isdigit('9') || isdigit('a'))
        return 2;
    if (!isspace(' ') || isspace('9'))
        return 3;
    if (!isxdigit('f') || !isxdigit('F') || isxdigit('g'))
        return 4;
    if (toupper('a') != 'A')
        return 5;
    if (tolower('Z') != 'z')
        return 6;
    return 0;
}
