/* SPDX-License-Identifier: MIT */
/* expect-error: use of 'S' with tag type that does not match previous declaration */
/* struct and union tags share one namespace (C89 3.1.2.3). */
struct S { int x; };
union S { int y; };

int main(void) {
    return 0;
}
