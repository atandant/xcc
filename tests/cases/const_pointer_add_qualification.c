/* SPDX-License-Identifier: MIT */
/* expect: 37 */
int main(void)
{
    int value = 37;
    int *mutable_pointer = &value;
    const int *const_pointer = mutable_pointer;
    return *const_pointer;
}
