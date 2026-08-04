/* SPDX-License-Identifier: MIT */
/* expect: 6 */
struct Bits { unsigned first : 3; unsigned second : 3; };
int main(void)
{
    volatile struct Bits bits;
    bits.first = 2;
    bits.second = 4;
    return bits.first + bits.second;
}
