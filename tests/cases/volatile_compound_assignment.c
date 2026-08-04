/* SPDX-License-Identifier: MIT */
/* expect: 21 */
int main(void)
{
    volatile int value;
    value = 5;
    value *= 4;
    value += 1;
    return value;
}
