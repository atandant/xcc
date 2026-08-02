/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* xcc-args: -Iinclude */
#include <float.h>

int main(void)
{
    if (FLT_RADIX != 2 || FLT_MANT_DIG != 24 || FLT_MAX < 1.0f)
        return 1;
    if (DBL_MANT_DIG != 53 || DBL_MAX < 1.0)
        return 2;
    if (LDBL_MANT_DIG != 64 || LDBL_MAX < 1.0L)
        return 3;
    return 0;
}
