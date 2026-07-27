/* SPDX-License-Identifier: MIT */
/* expect: 8 */
int main(void) {
    int values[2] = { 7, 8 };
    int (*row)[2] = { &values };
    return (*row)[1];
}
