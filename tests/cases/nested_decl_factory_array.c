/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int inc(char x) { return x + 1; }
int twice(char x) { return x * 2; }
int (*pick_inc(int ignored))(char) { return inc; }
int (*pick_twice(int ignored))(char) { return twice; }
int main(void)
{
    int (*(*factories[2])(int))(char);
    factories[0] = pick_inc;
    factories[1] = pick_twice;
    return factories[0](0)(3) + factories[1](0)(4);
}
