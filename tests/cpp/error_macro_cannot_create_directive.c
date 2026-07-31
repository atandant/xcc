/* SPDX-License-Identifier: MIT */
/* expect-error: stray '#' in program */
#define HASH #
HASH define VALUE 14
int main(void) { return 0; }
