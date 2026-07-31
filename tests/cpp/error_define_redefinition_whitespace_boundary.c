/* SPDX-License-Identifier: MIT */
/* expect-error: macro 'VALUE' redefined with different replacement */
#define VALUE 4+5
#define VALUE 4 +5
int main(void) { return VALUE; }
