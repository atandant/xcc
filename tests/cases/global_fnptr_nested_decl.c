/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int (*(*factory)(int))(char);
int add_six(char value) { return value + 6; }
int (*pick(int ignored))(char) { return add_six; }
int main(void) { factory = pick; return factory(0)(6); }
