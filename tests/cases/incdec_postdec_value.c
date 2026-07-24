/* SPDX-License-Identifier: MIT */
/* expect: 54 */
int main(void)
{
    int i = 5;
    int x = i--;

    return x * 10 + i;
}
