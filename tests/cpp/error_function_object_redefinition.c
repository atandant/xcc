/* SPDX-License-Identifier: MIT */
/* expect-error: macro 'VALUE' redefined with different replacement */
#define VALUE 1
#define VALUE(x) x
int main(void) { return 0; }
