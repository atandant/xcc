/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int *null_ptr(void) {
    return 0;
}

int main(void) {
    return null_ptr() == 0;
}
