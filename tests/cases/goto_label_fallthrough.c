/* SPDX-License-Identifier: MIT */
/* expect: 2 */
int main(void)
{
    int value;

    value = 1;
ordinary:
    value = 2;
    return value;
}
