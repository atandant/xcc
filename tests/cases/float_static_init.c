/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int value(void) { static float f = (2.0f + 1.5f) * 2; return (int)f; }
int main(void) { return value(); }
