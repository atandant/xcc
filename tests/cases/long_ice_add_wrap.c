/* SPDX-License-Identifier: MIT */
/* expect: 1 */
int main(void) {
    return (9223372036854775807L + 1L) < 0L;
}
