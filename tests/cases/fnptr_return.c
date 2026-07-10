/* SPDX-License-Identifier: MIT */
/* expect: 4 */
typedef int (*ip)(int);
int id(int x) { return x; }
ip pick(void) { return id; }
int main(void) { return pick()(4); }
