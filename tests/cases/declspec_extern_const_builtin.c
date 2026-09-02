/* SPDX-License-Identifier: MIT */
/* expect: 21 */
extern const char value;
const char value = 21;
int main(void) { return value; }
