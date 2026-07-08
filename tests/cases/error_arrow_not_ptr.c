/* SPDX-License-Identifier: MIT */
/* expect-error: cannot dereference non-pointer type 'struct Point' */
struct Point { int x; int y; };

int main(void) {
    struct Point p;
    return p->x;
}
