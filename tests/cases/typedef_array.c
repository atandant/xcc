/* SPDX-License-Identifier: MIT */
/* expect: 15 */
typedef int Row[3];

int main(void) {
    Row r;
    r[0] = 5;
    r[1] = 10;
    return r[0] + r[1];
}
