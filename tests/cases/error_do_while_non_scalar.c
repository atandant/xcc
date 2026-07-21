/* SPDX-License-Identifier: MIT */
/* expect-error: condition has non-scalar type 'struct S' */
struct S { int value; };

int main(void)
{
    struct S value;

    do {
        value.value = 1;
    } while (value);
    return 0;
}
