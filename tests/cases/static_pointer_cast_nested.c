/* SPDX-License-Identifier: MIT */
/* expect: 14 */
static void *pointer = (void *)(char *)14;
int main(void) { return (long)pointer; }
