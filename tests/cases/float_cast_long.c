/* SPDX-License-Identifier: MIT */
/* expect: 37 */
int main(void) { long x = 3000000037L; double d = (double)x; return (int)((long)d - 3000000000L); }
