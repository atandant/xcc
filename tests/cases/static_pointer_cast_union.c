/* SPDX-License-Identifier: MIT */
/* expect: 17 */
union Value { int *pointer; long bits; };
static union Value value = {(int *)17};
int main(void) { return value.bits; }
