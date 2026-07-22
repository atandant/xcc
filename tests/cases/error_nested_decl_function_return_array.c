/* SPDX-License-Identifier: MIT */
/* expect-error: function cannot return array type */
int bad(void)[3];
int main(void) { return 0; }
