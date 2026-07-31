/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) {
    return sizeof(4294967295U) == sizeof(unsigned int) &&
           sizeof(4294967296U) == sizeof(unsigned long);
}
