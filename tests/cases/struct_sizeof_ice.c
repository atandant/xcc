/* SPDX-License-Identifier: MIT */
/* expect: 2 */
/* Section 8.2: sizeof(struct S) in an array bound ICE */
struct S { int a; int b; };

int main(void) {
    int x[sizeof(struct S) / sizeof(int)];
    return (int)(sizeof x / sizeof(int));
}
