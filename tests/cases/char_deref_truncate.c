/* SPDX-License-Identifier: MIT */
/* expect: 44 */
int main(void) {
    char c;
    char *p;
    c = 0;
    p = &c;
    *p = 300;
    return c;
}
