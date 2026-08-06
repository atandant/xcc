/* SPDX-License-Identifier: MIT */
/* expect: 0 */

int main(void)
{
    int first = 5, second = first + 2, third = second * 2;
    return third == 14 ? 0 : 1;
}
