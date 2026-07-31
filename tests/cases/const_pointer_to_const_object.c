/* SPDX-License-Identifier: MIT */
/* expect: 17 */
int main(void)
{
    int value = 17;
    const int *const pointer = &value;
    return *pointer;
}
