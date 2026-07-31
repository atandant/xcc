/* SPDX-License-Identifier: MIT */
/* expect: 17 */
#if '\n' == 10 && (6 & 3) == 2 && (4 | 1) == 5 && (7 ^ 3) == 4 && \
    (1 << 4) == 16 && ~0 < 0
int main(void) { return 17; }
#endif
