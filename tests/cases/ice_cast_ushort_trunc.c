/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) {
    int *p;
    p = (unsigned short)65536;
    return p == 0;
}
