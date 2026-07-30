/* SPDX-License-Identifier: MIT */
/* expect: 18 */
int add_five(int value) { return value + 5; }
int times_three(int value) { return value * 3; }
static int (*operations[3])(int) = { add_five, times_three };

int main(void)
{
    return operations[0](4) + operations[1](2) +
           (operations[2] == 0) + 2;
}
