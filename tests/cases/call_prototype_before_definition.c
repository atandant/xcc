/* SPDX-License-Identifier: MIT */
/* expect: 14 */
int twice(int x);

int main(void) {
    return twice(7);
}

int twice(int x) {
    return x * 2;
}
