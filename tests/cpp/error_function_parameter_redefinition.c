/* SPDX-License-Identifier: MIT */
/* expect-error: macro 'ID' redefined with different replacement */
#define ID(x) x
#define ID(y) y
int main(void) { return 0; }
