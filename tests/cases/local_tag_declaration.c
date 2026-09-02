/* SPDX-License-Identifier: MIT */
/* expect: 0 */
typedef double Number;
typedef long Integer;

#define MAXALIGN Number n; double d; void *p; Integer i; long l

int main(void)
{
    struct Container { char c; union { MAXALIGN; } value; };
    struct Container object;

    object.value.i = 17;
    return object.value.i != 17;
}
