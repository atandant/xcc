/* SPDX-License-Identifier: MIT */
/* expect: 15 */
int main(void) {
    return sizeof(char) + sizeof(short) + sizeof(int) + sizeof(long);
}
