/* SPDX-License-Identifier: MIT */
/* expect: 16 */
static int *pointers[2] = {(int *)0, (int *)16};
int main(void) { return pointers[0] == 0 ? (long)pointers[1] : 1; }
