/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) {
    int *p;
    p = (void *)(1 - 1);
    return p == 0;
}
