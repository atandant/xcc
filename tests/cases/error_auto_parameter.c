/* SPDX-License-Identifier: MIT */
/* expect-error: invalid storage class for parameter */
int f(auto int x) { return x; }
int main(void) { return f(1); }
