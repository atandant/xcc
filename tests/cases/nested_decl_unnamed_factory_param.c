/* SPDX-License-Identifier: MIT */
/* expect: 7 */
int inc(char x) { return x + 1; }
int (*pick(int ignored))(char) { return inc; }
int invoke(int (*(*)(int))(char));
int invoke(int (*(*factory)(int))(char)) { return factory(0)(6); }
int main(void) { return invoke(pick); }
