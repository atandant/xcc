/* SPDX-License-Identifier: MIT */
/* expect-error: no member named 'z' in 'struct Point' */
struct Point { int x; int y; };

int main(void) {
    struct Point p;
    return p.z;
}
