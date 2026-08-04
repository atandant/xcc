/* SPDX-License-Identifier: MIT */
/* expect: 14 */
int main(void)
{
    volatile int value;
    int old;
    value = 6;
    old = value++;
    ++value;
    return old + value;
}
