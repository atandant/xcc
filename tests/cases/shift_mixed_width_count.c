/* SPDX-License-Identifier: MIT */
/* expect: 23 */
int main(void)
{
    unsigned long value = 1UL << 40;
    int count = 8;

    value >>= count;
    return value == (1UL << 32) ? 23 : 1;
}
