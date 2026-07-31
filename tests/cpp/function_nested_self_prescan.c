/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int f(int x) { return x; }
#define VALUE 2
#define f(x) f(VALUE * (x))
int main(void) { return f(f(1)); }
