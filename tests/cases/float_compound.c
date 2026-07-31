/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int main(void) { float f = 3.0f; f += 2.0; f *= 4; f -= 2.0f; f /= 2.0f; return (int)f; }
