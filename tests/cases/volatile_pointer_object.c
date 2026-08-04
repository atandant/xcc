/* SPDX-License-Identifier: MIT */
/* expect: 19 */
int main(void)
{
    int first;
    int second;
    int * volatile pointer;
    first = 7;
    second = 19;
    pointer = &first;
    pointer = &second;
    return *pointer;
}
