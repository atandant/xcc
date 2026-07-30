/* SPDX-License-Identifier: MIT */
/* expect: 17 */
int add_two(int value) { return value + 2; }

int main(void)
{
    int (*fn)(int) = add_two;
    return (0, fn)(15);
}
