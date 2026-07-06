/* SPDX-License-Identifier: MIT */
/* expect: 16 */
int main(void) {
    return sizeof(int *) + sizeof(char *);
}
