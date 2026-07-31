/* SPDX-License-Identifier: MIT */
/* expect: 7 */
#define ALIAS VALUE
#define VALUE 7
#undef ALIAS
int main(void) { return VALUE; }
