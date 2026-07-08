/* SPDX-License-Identifier: MIT */
/* expect: 3 */
int main(void) {
    typedef int Foo;
    Foo x;
    x = 3;
    return x;
}
