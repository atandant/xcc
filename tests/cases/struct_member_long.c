/* SPDX-License-Identifier: MIT */
/* expect: 42 */
/* Section 3: long member load/store */
struct Data { long v; };

int main(void) {
    struct Data d;
    d.v = 42;
    return (int)d.v;
}
