/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int inc(int x) { return x + 1; }
int twice(int x) { return x * 2; }
int main(void)
{
    int (*ops[2])(int);
    int (*(*table)[2])(int);
    ops[0] = inc;
    ops[1] = twice;
    table = &ops;
    return (*table)[0](3) + (*table)[1](4);
}
