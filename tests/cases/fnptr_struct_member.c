/* SPDX-License-Identifier: MIT */
/* expect: 6 */
struct S { int (*cb)(int); };
int triple(int x) { return x * 3; }
int main(void) {
    struct S s;
    s.cb = triple;
    return s.cb(2);
}
