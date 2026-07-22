/* SPDX-License-Identifier: MIT */
/* expect-error: function cannot return function type */
int bad(void)(int);
int main(void) { return 0; }
