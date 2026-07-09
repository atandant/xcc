/* SPDX-License-Identifier: MIT */
/* expect: 99 */
/* Union nested in a struct: the discriminated-union pattern. */
union Val { int i; long l; };
struct Box { int tag; union Val v; };

int main(void) {
    struct Box b;
    b.tag = 1;
    b.v.i = 99;
    return b.v.i;
}
