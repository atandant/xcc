/* SPDX-License-Identifier: MIT */
/* expect-warning: call to function pointer without a prototype */
int old();
int old() { return 3; }
int main(void) {
    int (*fp)();
    fp = old;
    return fp(1, 2);
}
