/* SPDX-License-Identifier: MIT */
/* expect: 12 */
typedef int Number;
extern volatile Number value;
volatile Number value = 12;
int main(void) { return value; }
