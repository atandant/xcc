/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int call(void) { int helper(int); return helper(4); }
int helper(int value) { return value + 5; }
int main(void) { return call() - 9; }
