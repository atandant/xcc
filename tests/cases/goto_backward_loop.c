/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int main(void)
{
    int i;

    i = 0;
again:
    i = i + 1;
    if (i < 5)
        goto again;
    return i;
}
