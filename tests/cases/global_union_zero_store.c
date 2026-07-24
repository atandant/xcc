/* SPDX-License-Identifier: MIT */
/* expect: 55 */
union Value { int number; long wide; };
union Value value;
int main(void) { value.wide = 55; return value.number; }
