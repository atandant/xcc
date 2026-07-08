/* SPDX-License-Identifier: MIT */
/* expect: 3 */
/* Section 4.4: scalar member may use single-element brace (C89 3.5.7) */
struct Box { int v; };

int main(void) {
    struct Box b = { { 3 } };
    return b.v;
}
