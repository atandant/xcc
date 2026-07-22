/* SPDX-License-Identifier: MIT */
/* expect: 13 */
struct Dispatch { int (*ops[2])(int); };
int inc(int x) { return x + 1; }
int twice(int x) { return x * 2; }
int main(void)
{
    struct Dispatch d;
    d.ops[0] = inc;
    d.ops[1] = twice;
    return d.ops[0](4) + d.ops[1](4);
}
