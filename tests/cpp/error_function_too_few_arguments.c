/* SPDX-License-Identifier: MIT */
/* expect-error: macro 'ADD' requires 2 arguments, but 1 given */
#define ADD(a, b) ((a) + (b))
int main(void) { return ADD(1); }
