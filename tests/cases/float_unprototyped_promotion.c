/* SPDX-License-Identifier: MIT */
/* expect: 6 */
int take(double x) { return (int)x; }
int main(void) { int (*fn)() = take; float f = 6.75f; return fn(f); }
