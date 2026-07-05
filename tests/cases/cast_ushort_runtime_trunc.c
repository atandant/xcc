/* SPDX-License-Identifier: MIT */
/* expect: 255 */
int main(void) {
    int x;
    unsigned short s;
    x = 65535;
    s = (unsigned short)x;
    return (int)(s - (unsigned short)65280);
}
