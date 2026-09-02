/* SPDX-License-Identifier: MIT */
/* expect: 11 */
typedef int Number;
extern const Number value;
const Number value = 11;
int main(void) { return value; }
