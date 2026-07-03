/* SPDX-License-Identifier: MIT */
/* expect-error: incompatible types assigning 'long' to 'int *' */
int main(void) {
    int *p;
    p = 9223372036854775807L + 1L;
    return 0;
}
