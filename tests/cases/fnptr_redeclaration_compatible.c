/* SPDX-License-Identifier: MIT */
/* expect: 29 */
int transform(int);
int (*selected)(int) = transform;

int transform(int value) { return value + 9; }

int main(void) { return selected(20); }
