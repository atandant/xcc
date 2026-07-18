/* SPDX-License-Identifier: MIT */
/* expect: 0 */

int identities(int x)
{
    if (x + 0 != x) return 1;
    if (0 + x != x) return 2;
    if (x - 0 != x) return 3;
    if (x - x != 0) return 4;
    if (x * 1 != x) return 5;
    if (1 * x != x) return 6;
    if (x * 0 != 0) return 7;
    if (0 * x != 0) return 8;
    if (x / 1 != x) return 9;
    if (x % 1 != 0) return 10;
    if ((x & 0) != 0) return 11;
    if ((x & -1) != x) return 12;
    if ((x | 0) != x) return 13;
    if ((x ^ 0) != x) return 14;
    if ((x << 0) != x) return 15;
    return 0;
}

unsigned long unsigned_identities(unsigned long x)
{
    return (x / (unsigned long)1) + (x % (unsigned long)1);
}

int main(void)
{
    int result;

    result = identities(-2147483647 - 1);
    if (result != 0)
        return result;
    return unsigned_identities((unsigned long)0xffffffffL) ==
           (unsigned long)0xffffffffL ? 0 : 16;
}
