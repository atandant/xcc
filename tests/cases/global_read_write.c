/* SPDX-License-Identifier: MIT */
/* expect: 42 */
int value;
int set_value(int n) { value = n; return value; }
int main(void) { return set_value(42); }
