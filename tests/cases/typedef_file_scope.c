/* SPDX-License-Identifier: MIT */
/* expect: 11 */
typedef int Val;

Val id(Val x) {
    return x;
}

int main(void) {
    return id(11);
}
