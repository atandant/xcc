/* SPDX-License-Identifier: MIT */
/* expect: 2 */
int next(int *count) {
    *count = *count + 1;
    return *count;
}

int main(void) {
    int count;
    int value;
    count = 0;
    value = (next(&count), next(&count));
    if (count != 2)
        return 99;
    return value;
}
