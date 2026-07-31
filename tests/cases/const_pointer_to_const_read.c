/* SPDX-License-Identifier: MIT */
/* expect: 31 */
int main(void)
{
    int value = 31;
    const int *pointer = &value;
    return *pointer;
}
