/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) {
    unsigned int u;
    unsigned long l;
    u = 0 - (unsigned int)1;
    l = (unsigned long)u;
    return l == 4294967295L;
}
