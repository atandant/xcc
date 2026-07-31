/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types assigning 'const int *' to 'int *' */
int main(void)
{
    const int value = 1;
    const int *source = &value;
    int *destination;
    destination = source;
    return *destination;
}
