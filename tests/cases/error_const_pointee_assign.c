/* SPDX-License-Identifier: MIT */
/* expect-error: assignment to const-qualified object */
int main(void)
{
    int value = 1;
    const int *pointer = &value;
    *pointer = 2;
    return value;
}
