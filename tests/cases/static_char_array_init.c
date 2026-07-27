/* SPDX-License-Identifier: MIT */
/* expect: 7 */
static char msg[] = "hello";
int main(void) { return msg[0] + msg[4] - 208; }
