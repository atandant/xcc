/* SPDX-License-Identifier: MIT */
/* expect: 198 */
int main(void) {
    char c[3] = {65, 66, 67};
    return c[0] + c[1] + c[2];
}
