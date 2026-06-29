/* SPDX-License-Identifier: MIT */
/* expect: 12 */
int main(void) {
    /* skip
       these tokens: return 99;
       and keep scanning */
    return 12;
}
