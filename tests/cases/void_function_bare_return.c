/* SPDX-License-Identifier: MIT */
/* expect: 3 */
void noop(void) {
    return;
}

int main(void) {
    noop();
    return 3;
}
