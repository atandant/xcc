/* SPDX-License-Identifier: MIT */
/* expect: 11 */
int apply(int x, int (*fn)(int)) { return fn(x); }
int inc(int x) { return x + 1; }
int main(void) { return apply(10, inc); }
