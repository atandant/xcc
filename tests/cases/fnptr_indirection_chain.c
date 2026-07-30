/* SPDX-License-Identifier: MIT */
/* expect: 19 */
int add_four(int value) { return value + 4; }

int main(void)
{
    int (*fn)(int) = add_four;
    int (**slot)(int) = &fn;
    return (**slot)(15);
}
