/* SPDX-License-Identifier: MIT */
/* expect-error: struct nesting exceeds translation limit of 6 levels */
struct L7 { int x; };
struct L6 { struct L7 m; };
struct L5 { struct L6 m; };
struct L4 { struct L5 m; };
struct L3 { struct L4 m; };
struct L2 { struct L3 m; };
struct L1 { struct L2 m; };
struct L0 { struct L1 m; };

int main(void) {
    return sizeof(struct L0);
}
