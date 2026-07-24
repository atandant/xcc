/* SPDX-License-Identifier: MIT */
/* expect: 35 */
int main(void)
{
    int i = 3;
    int a;
    int b;

    a = i++;
    b = ++i;
    return a * 10 + b;
}
