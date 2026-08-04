/* SPDX-License-Identifier: MIT */
/* expect: 33 */
int main(void)
{
    int value;
    int *plain;
    volatile int *qualified;
    value = 33;
    plain = &value;
    qualified = plain;
    return *qualified;
}
