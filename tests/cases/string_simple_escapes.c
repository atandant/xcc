/* SPDX-License-Identifier: MIT */
/* expect: 73 */
int main(void) {
    char *p = "\'\"\?\\\a\b\f\n\r\t\v";
    if (p[0] == 39 && p[1] == 34 && p[2] == 63 && p[3] == 92 &&
        p[4] == 7 && p[5] == 8 && p[6] == 12 && p[7] == 10 &&
        p[8] == 13 && p[9] == 9 && p[10] == 11)
        return 73;
    return 1;
}
