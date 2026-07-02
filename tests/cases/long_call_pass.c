/* SPDX-License-Identifier: MIT */
/* expect: 7 */
long add3(long a, long b, long c) { return a + b + c; }
int main(void) { return add3(1L, 2L, 4L); }
