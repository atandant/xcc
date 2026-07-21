/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int main(void)
{
    int value;

    value = 7;
    do {
        value = value + 1;
    } while (0);
    return value;
}
