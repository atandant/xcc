/* SPDX-License-Identifier: MIT */
/* expect: 7 */
typedef int (*(*Factory)(int))(char);
struct Holder { Factory make; };
int inc(char x) { return x + 1; }
int (*pick(int ignored))(char) { return inc; }
int invoke(Factory make) { return make(0)(6); }
int main(void)
{
    struct Holder h;
    h.make = pick;
    return invoke(h.make);
}
