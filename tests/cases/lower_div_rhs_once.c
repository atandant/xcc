/* SPDX-License-Identifier: MIT */
/* expect: 4 */
int divisor(int *count) {
    *count = *count + 1;
    return 2;
}

int main(void) {
    int count;
    int value;
    count = 0;
    value = 8 / divisor(&count);
    if (count != 1)
        return 99;
    return value;
}
