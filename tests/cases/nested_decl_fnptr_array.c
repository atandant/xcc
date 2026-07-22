/* SPDX-License-Identifier: MIT */
/* expect: 13 */
int inc(int x) { return x + 1; }
int twice(int x) { return x * 2; }
int main(void)
{
    int (*ops[2])(int);
    ops[0] = inc;
    ops[1] = twice;
    return ops[0](4) + ops[1](4);
}
