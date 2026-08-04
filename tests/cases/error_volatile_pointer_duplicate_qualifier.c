/* SPDX-License-Identifier: MIT */
/* expect-error: duplicate 'volatile' type qualifier */
int main(void)
{
    int * volatile volatile pointer;
    return pointer != 0;
}
