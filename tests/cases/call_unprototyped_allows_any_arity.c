/* SPDX-License-Identifier: MIT */
/* expect: 11 */
int pick();

int main(void) {
    return pick(11, 22, 33);
}

int pick(int x) {
    return x;
}
