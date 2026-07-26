/* SPDX-License-Identifier: MIT */
/* expect: 232 */
struct Entry { char *name; int value; };
struct Entry entries[] = { { "one", 1 }, { "two", 2 } };
int main(void) { return entries[0].name[0] + entries[1].name[1] + entries[1].value; }
