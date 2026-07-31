/* SPDX-License-Identifier: MIT */
/* expect: 9 */
static const float base = 1.25f;
float add(const float x) { return x + base; }
int main(void) { const float f = 3.25f; const double d = 4.5; return (int)(add(f) + d); }
