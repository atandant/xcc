/* SPDX-License-Identifier: MIT */
/* expect-warning: call to function 'g' without a prototype */
int g();
int main(void) { return g(); }
int g(void) { return 0; }
