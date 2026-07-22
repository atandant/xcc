/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int inc(char x) { return x + 1; }
int (*pick(int ignored))(char) { return inc; }
int main(void)
{
    return ((int (*(*)(int))(char))pick)(0)(6);
}
