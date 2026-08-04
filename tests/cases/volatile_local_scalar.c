/* SPDX-License-Identifier: MIT */
/* expect: 42 */
int main(void)
{
    volatile int value;
    value = 41;
    value = value + 1;
    return value;
}
