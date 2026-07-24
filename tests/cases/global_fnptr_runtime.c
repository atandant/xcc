/* SPDX-License-Identifier: MIT */
/* expect: 9 */
int (*callback)(int);
int increment(int n) { return n + 1; }
int main(void) { callback = increment; return callback(8); }
