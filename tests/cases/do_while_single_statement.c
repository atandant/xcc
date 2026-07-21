/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int main(void)
{
    int i;

    i = 0;
    do
        i = i + 2;
    while (i < 5);
    return i;
}
