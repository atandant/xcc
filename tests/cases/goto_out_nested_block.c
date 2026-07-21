/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void)
{
    int value;

    value = 1;
    {
        value = value + 2;
        goto done;
        value = 99;
    }
done:
    return value;
}
