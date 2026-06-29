/* SPDX-License-Identifier: MIT */
/* expect: 5 */
int touch(int x) {
    return x + 10;
}

int main(void) {
    touch(99);
    return 5;
}
