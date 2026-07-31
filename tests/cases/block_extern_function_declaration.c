/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int call(void) { extern int helper(int); return helper(6); }
int helper(int value) { return value + 2; }
int main(void) { return call() - 8; }
