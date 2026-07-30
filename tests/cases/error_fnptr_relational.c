/* SPDX-License-Identifier: MIT */
/* expect-error: invalid operands to relational operator */
int first(void) { return 1; }
int second(void) { return 2; }

int main(void) { return first < second; }
