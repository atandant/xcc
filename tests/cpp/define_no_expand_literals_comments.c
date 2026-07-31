/* SPDX-License-Identifier: MIT */
/* expect: 1 */
#define NAME 99
int main(void) {
    /* NAME must not expand here. */
    return "NAME"[0] == 'N' && 'N' != NAME;
}
