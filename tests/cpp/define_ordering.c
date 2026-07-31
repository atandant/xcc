/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int VALUE(void) { return 3; }
int before(void) { return VALUE(); }
#define VALUE 9
int main(void) { return before() + VALUE; }
