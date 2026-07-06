/* SPDX-License-Identifier: MIT */
/* expect: 22 */
int main(void) {
    return sizeof(unsigned short) + sizeof(int[5]);
}
