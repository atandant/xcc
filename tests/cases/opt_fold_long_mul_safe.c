/* SPDX-License-Identifier: MIT */
/* expect: 42 */
int main(void) {
    return (9223372036854775807L / 9223372036854775807L) * 42;
}
