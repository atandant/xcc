/* SPDX-License-Identifier: MIT */
/* expect-error: extra tokens at end of #undef directive */
#define VALUE 1
#undef VALUE extra
int main(void) { return VALUE; }
