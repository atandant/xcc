/* SPDX-License-Identifier: MIT */
/* expect: 0 */
#include <stdint.h>

int main(void)
{
    int8_t i8 = INT8_MIN;
    uint16_t u16 = UINT16_MAX;
    int32_t i32 = INT32_C(2147483647);
    uint64_t u64 = UINT64_C(18446744073709551615);
    intptr_t ip = (intptr_t)&i8;

    if (sizeof(i8) != 1 || sizeof(u16) != 2)
        return 1;
    if (sizeof(i32) != 4 || sizeof(u64) != 8)
        return 2;
    if (i8 != -128 || u16 != 65535U)
        return 3;
    if (i32 != INT32_MAX || u64 != UINT64_MAX)
        return 4;
    if ((int8_t)UINT8_MAX != -1)
        return 5;
    if ((void *)ip != (void *)&i8)
        return 6;
    if (INT_FAST16_MAX < INT16_MAX || UINT_FAST32_MAX < UINT32_MAX)
        return 7;
    return 0;
}
