/* SPDX-License-Identifier: MIT */
/* expect-error: variable 'u' has incomplete type 'union U' */
/* Cannot define an object of an incomplete union type. */
union U;

int main(void) {
    union U u;
    return 0;
}
