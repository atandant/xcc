/* SPDX-License-Identifier: MIT */
/* expect: 90 */
union Value { char text[4]; int number; };
union Value value = { "AZ" };
int main(void) { return value.text[1]; }
