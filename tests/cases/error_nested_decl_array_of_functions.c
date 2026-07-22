/* SPDX-License-Identifier: MIT */
/* expect-error: array element type must not be a function type */
int bad[3](int);
int main(void) { return 0; }
