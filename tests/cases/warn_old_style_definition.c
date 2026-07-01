/* SPDX-License-Identifier: MIT */
/* expect-warning: function 'f' defined without a prototype */
int f() { return 0; }
int main(void) { return f(); }
