/* SPDX-License-Identifier: MIT */
/* expect: 45 */
enum Value { VALUE = (5 << 3) | (7 ^ 2) };
int main(void) { return VALUE; }
