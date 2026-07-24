/* SPDX-License-Identifier: MIT */
/* expect: 7 */
struct Bits { unsigned int value : 4; };
struct Bits bits;
int main(void) { bits.value = 7; return bits.value; }
