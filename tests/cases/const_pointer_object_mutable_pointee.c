/* SPDX-License-Identifier: MIT */
/* expect: 24 */
int main(void)
{
    int value = 9;
    int *const pointer = &value;
    *pointer = 24;
    return value;
}
