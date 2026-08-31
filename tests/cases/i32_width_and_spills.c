/* SPDX-License-Identifier: MIT */
/* expect: 0 */

static int signed_id(int x) { return x; }
static unsigned unsigned_id(unsigned x) { return x; }
static int opaque(int x) { return x; }

static unsigned spill_pressure(unsigned seed)
{
    unsigned a = seed + 1, b = seed + 2, c = seed + 3;
    unsigned d = seed + 4, e = seed + 5, f = seed + 6;
    unsigned g = seed + 7, h = seed + 8, i = seed + 9;
    unsigned j = seed + 10, k = seed + 11, l = seed + 12;
    int extra = opaque(3);

    return a + b + c + d + e + f + g + h + i + j + k + l + extra;
}

int main(void)
{
    if (signed_id(-1) != -1)
        return 1;
    if ((long)signed_id(-2147483647 - 1) != -2147483648L)
        return 2;
    if ((unsigned long)unsigned_id(0xffffffffU) != 4294967295UL)
        return 3;
    if (spill_pressure(0xfffffff0U) != 0xffffff91U)
        return 4;
    return 0;
}
