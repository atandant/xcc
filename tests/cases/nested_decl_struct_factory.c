/* SPDX-License-Identifier: MIT */
/* expect: 7 */
struct Factory { int (*(*make)(int))(char); };
int inc(char x) { return x + 1; }
int (*pick(int ignored))(char) { return inc; }
int main(void)
{
    struct Factory f;
    f.make = pick;
    return f.make(0)(6);
}
