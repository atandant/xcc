/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int VALUE(void) { return 5; }
#undef UNKNOWN
#define VALUE 3
#undef VALUE
int after(void) { return VALUE(); }
#define VALUE 7
int main(void) { return after() + VALUE; }
