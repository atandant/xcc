/* SPDX-License-Identifier: MIT */
/* expect: 0 */
/* xcc-args: -Wno-implicit-function-declaration */
int main(void) { return foo(); }
int foo(void) { return 0; }
