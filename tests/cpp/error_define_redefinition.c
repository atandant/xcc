/* SPDX-License-Identifier: MIT */
/* expect-error: macro 'VALUE' redefined with different replacement */
#define VALUE 1
#define VALUE 2
int main(void) { return VALUE; }
