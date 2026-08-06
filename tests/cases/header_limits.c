/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <limits.h>

int main(void)
{
    if (CHAR_BIT != 8)
        return 1;
    if (SCHAR_MIN > -128 || SCHAR_MAX < 127)
        return 2;
    if (UCHAR_MAX != 255)
        return 3;
    if (SHRT_MIN > -32768 || SHRT_MAX < 32767)
        return 4;
    if (USHRT_MAX != 65535U)
        return 5;
    if (INT_MAX != 2147483647)
        return 6;
    if (UINT_MAX != 4294967295U)
        return 7;
    if (LONG_MAX != 9223372036854775807L)
        return 8;
    if (ULONG_MAX != 18446744073709551615UL)
        return 9;
    if (MB_LEN_MAX < 1)
        return 10;
    return 0;
}
