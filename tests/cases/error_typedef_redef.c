/* SPDX-License-Identifier: MIT */
/* expect-error: redefinition of typedef 'Foo' */
typedef int Foo;
typedef char Foo;

int main(void) {
    return 0;
}
