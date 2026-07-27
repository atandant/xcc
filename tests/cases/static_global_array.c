/* SPDX-License-Identifier: MIT */
/* expect: 0 */
static int a[3] = {1, 2, 3};
int main(void) { return a[0] + a[1] + a[2] - 6; }
