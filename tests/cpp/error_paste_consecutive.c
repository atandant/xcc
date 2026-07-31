/* SPDX-License-Identifier: MIT */
/* expect-error: invalid '##' placement in macro replacement list */
#define BAD left ## ## right
int main(void) { return 0; }
