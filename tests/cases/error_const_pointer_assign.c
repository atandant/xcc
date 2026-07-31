/* SPDX-License-Identifier: MIT */
/* expect-error: assignment to const-qualified object */
int main(void)
{
    int first = 1;
    int second = 2;
    int *const pointer = &first;
    pointer = &second;
    return *pointer;
}
