/* SPDX-License-Identifier: MIT */
/* expect: 18 */
int main(void)
{
    static int *pointer = (int *)18;
    return (long)pointer;
}
