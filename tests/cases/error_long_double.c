/* SPDX-License-Identifier: MIT */
/* expect-error: long double is not supported */
int main(void) { long double x; return sizeof(x); }
