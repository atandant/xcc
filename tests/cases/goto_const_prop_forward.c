/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void)
{
    int value;

    value = 1;
    goto done;
    value = 2;
done:
    return value;
}
