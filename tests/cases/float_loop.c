/* SPDX-License-Identifier: MIT */
/* expect: 15 */
int main(void) { float x = 0.0f; int n = 0; while (x < 5.0f) { x += 0.5f; n++; } for (; x > 0.0f; x -= 1.0f) n++; return n; }
