/* SPDX-License-Identifier: MIT */
/* expect: 0 */
int main(void) { static int value = 8; static int *pointer = &value; return *pointer - 8; }
