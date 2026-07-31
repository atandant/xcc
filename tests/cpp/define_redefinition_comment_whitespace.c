/* SPDX-License-Identifier: MIT */
/* expect: 9 */
#define VALUE 4 + 5
#define VALUE 4/**/+/**/5
int main(void) { return VALUE; }
