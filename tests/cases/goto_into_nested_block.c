/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void)
{
    int value;

    value = 1;
    goto inside;
    {
        value = 99;
inside:
        value = value + 2;
    }
    return value;
}
