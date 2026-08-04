/* SPDX-License-Identifier: MIT */
/* expect: 31 */
const volatile int value = 31;
int main(void)
{
    const volatile int *pointer;
    pointer = &value;
    return *pointer;
}
