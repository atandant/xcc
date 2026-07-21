/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int main(void)
{
    int value;

    value = 7;
    goto done;
    value = 99;
done:
    return value;
}
