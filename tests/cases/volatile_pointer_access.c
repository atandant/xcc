/* SPDX-License-Identifier: MIT */
/* expect: 23 */
int main(void)
{
    int value;
    volatile int *pointer;
    value = 11;
    pointer = &value;
    *pointer = *pointer + 12;
    return value;
}
