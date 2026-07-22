/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int inc(char x) { return x + 1; }
int (*pick(void))(char) { return inc; }
int main(void) { return pick()(6); }
