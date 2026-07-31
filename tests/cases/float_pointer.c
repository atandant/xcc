/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int main(void) { double d = 3.25; double *p = &d; *p = *p + 5.0; return (int)d; }
